(in-package :clasp-cleavir)

;;; Generates code that checks whether the unwind is to this frame.
;;; If it is, it returns the control point index, i.e. which catch to jump to.
;;; Otherwise it rethrows, going to the provided landing pad.
;;; An alternate strategy would be to test whether it's an unwind, and if not, cleanup and/or resume
;;; and if so, extract the go-index and jump table and so on in a catch block.
;;; Saves a landing pad and my comprehension.
(defun generate-match-unwind (return-value abi frame landing-pad-for-unwind-rethrow exn.slot)
  ;; We do this weird thing to get the index while still generating the other code around it.
  ;; FIXME: with-begin-end-catch should maybe abstract away so it can return an actual thing.
  (let (go-index)
    (cmp:with-begin-end-catch ((%load exn.slot "exn") exception-ptr nil)
      ;; Check if frame is correct against tagbody and jump to jumpid
      (cmp:with-landing-pad landing-pad-for-unwind-rethrow
        (setq go-index
              (%intrinsic-invoke-if-landing-pad-or-call
               "cc_landingpadUnwindMatchFrameElseRethrow" 
               (list exception-ptr frame)
               "go-index"))
        ;; Restore multiple values before going to whichever block.
        (with-return-values (return-vals return-value abi)
          (%intrinsic-call "cc_restoreMultipleValue0" (list return-value)))))
    go-index))

;;; Generates a landing pad and code to deal with unwinds to this function.
;;; See note on previous and possibly future operation below for the purpose of the first argument,
;;; which as of this writing will always be NIL.
(defun generate-catch-landing-pad (maybe-cleanup-landing-pad not-unwind-block exn.slot ehselector.slot
                                   return-value abi tags catches frame)
  (assert (not (null catches)))
  ;; Bind a fresh IRBuilder so we don't fuck with whatever our caller's been doing.
  (cmp:with-irbuilder ((llvm-sys:make-irbuilder cmp:*llvm-context*))
    (let ((landing-pad-for-unwind-rethrow
            (cmp::generate-rethrow-landing-pad cmp:*current-function* not-unwind-block
                                               nil ; '(cmp::typeid-core-unwind)
                                               exn.slot ehselector.slot))
          (landing-pad-block (cmp:irc-basic-block-create "catch-landing-pad")))
      (cmp:irc-begin-block landing-pad-block)
      (let* ((lpad (cmp:irc-create-landing-pad 1 "lp"))
             (exception-structure (cmp:irc-extract-value lpad (list 0) "exception-structure"))
             (exception-selector (cmp:irc-extract-value lpad (list 1) "exception-selector")))
        (%store exception-structure exn.slot)
        (%store exception-selector ehselector.slot)
        (cmp:irc-add-clause lpad (cmp:irc-exception-typeid* 'cmp:typeid-core-unwind))
        ;; we have to go to the cleanup later, so we can't ignore any exceptions.
        (when maybe-cleanup-landing-pad (llvm-sys:set-cleanup lpad t))
        (let* ((is-unwind-block (cmp:irc-basic-block-create "match-unwind"))
               (typeid (%intrinsic-call "llvm.eh.typeid.for"
                                        (list (cmp:irc-exception-typeid* 'cmp:typeid-core-unwind))))
               (matches-type (cmp:irc-icmp-eq exception-selector typeid)))
          ;; If the exception is Clasp's UnwindException, we handle it.
          ;; Otherwise we go to the cleanup, or perhaps directly to the resume.
          (cmp:irc-cond-br matches-type is-unwind-block not-unwind-block)
          (cmp:irc-begin-block is-unwind-block)
          (let* ((go-index (generate-match-unwind
                            return-value abi frame landing-pad-for-unwind-rethrow exn.slot))
                 (default-block (cmp:irc-basic-block-create "switch-default"))
                 (sw (cmp:irc-switch go-index default-block (length catches))))
            (loop for catch in catches
                  for jump-id = (cc-mir:go-index catch)
                  do (let* ((target (second (cleavir-ir:successors catch)))
                            (tag-block (gethash target tags)))
                       (assert (not (null tag-block)))
                       (llvm-sys:add-case sw (%size_t jump-id) tag-block)))
            (cmp:irc-begin-block default-block)
            (%intrinsic-invoke-if-landing-pad-or-call "throwIllegalSwitchValue"
                                                      (list go-index (%size_t (length catches)))
                                                      ""
                                                      maybe-cleanup-landing-pad)
            (cmp:irc-unreachable))))
      landing-pad-block)))

(defun generate-resume-block (exn.slot ehselector.slot)
  (let* ((ehbuilder       (llvm-sys:make-irbuilder cmp:*llvm-context*))
         (ehresume        (cmp:irc-basic-block-create "ehresume"))
         (_               (cmp:irc-set-insert-point-basic-block ehresume ehbuilder))
         (exn7            (llvm-sys:create-load-value-twine ehbuilder exn.slot "exn7"))
         (sel             (llvm-sys:create-load-value-twine ehbuilder ehselector.slot "sel"))
         (undef           (llvm-sys:undef-value-get cmp:%exception-struct% ))
         (lpad.val        (llvm-sys:create-insert-value ehbuilder undef exn7 '(0) "lpad.val"))
         (lpad.val8       (llvm-sys:create-insert-value ehbuilder lpad.val sel '(1) "lpad.val8"))
         (_               (llvm-sys:create-resume ehbuilder lpad.val8)))
    (declare (ignore _))
    ehresume))

(defun alloca-exn.slot ()
  (alloca-i8* "exn.slot"))
(defun alloca-ehselector.slot ()
  (alloca-i32 "ehselector.slot"))

;;; Note: Previously, we had cleanup blocks in almost every function, which popped a "shadow stack"
;;; loaded with arguments for the debugger. Stackmaps mean we no longer have to do that, but similar
;;; code may be useful for inlining UNWIND-PROTECT.
;;; Essentially it worked as follows: We'd bind some special variables with exn.slot, ehselector.slot,
;;; a cleanup landing pad, and the cleanup block that succeded the pad. The landing pad would be used
;;; for basically all calls (requiring them to be INVOKEs), and also passed to
;;; generate-catch-landing-pad with the others. If there was a cleanup pad, the catch's pad would be
;;; marked as a cleanup (you can still see this code above) and during an exit would branch to the
;;; cleanup block. All pads in the function would share ehselector and exn slots so that they could
;;; all go to the same cleanup block and thus the same resume.

;; For saving pads between multiple functions using them
(defvar *catch-pads-table*)

;;; "unreal" catches are catch instructions produced in HIR that lie around in invoke-instructions
;;; despite being deleted as useless. Most catches probably turn out to be unreal.
(defun real-catches (catches)
  (remove-if-not (lambda (catch) (typep catch 'cc-mir:assign-catch-instruction)) catches))

(defmacro with-catch-pad-prep (&body body)
  `(let ((*catch-pads-table* (make-hash-table :test #'equal)))
     ,@body))

;;; Note: NOT like LLVM "catchpad" instruction.
(defun catch-pad (catches return-value abi tags function-info)
  (or (gethash catches *catch-pads-table*)
      (setf (gethash catches *catch-pads-table*)
            (let ((real (real-catches catches)))
              (if (null real)
                  ;; no real catches or need to invoke
                  nil
                  ;; hard part
                  (let* ((exn.slot (alloca-exn.slot))
                         (ehselector.slot (alloca-ehselector.slot))
                         (cleanup-block (generate-resume-block exn.slot ehselector.slot))
                         (frame (frame-value function-info)))
                    (generate-catch-landing-pad
                     nil cleanup-block exn.slot ehselector.slot
                     return-value abi tags real frame)))))))
