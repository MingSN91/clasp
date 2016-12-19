/*
    File: bitVector.h
*/

/*
Copyright (c) 2014, Christian E. Schafmeister

CLASP is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

See directory 'clasp/licenses' for full details.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/* -^- */
#ifndef core_BitVector_H
#define core_BitVector_H
#include <stdio.h>
#include <limits>
#include <string>
#include <vector>
#include <set>
#include <clasp/core/foundation.h>
#include <clasp/core/object.h>
#include <clasp/core/lispVector.h>

namespace core {

  SMART(BitVector);

  class BitVector_O : public Vector_O {
    friend T_sp core__bit_array_op(T_sp o, T_sp x, T_sp y, T_sp r);
    LISP_CLASS(core, ClPkg, BitVector_O, "bit-vector",Vector_O);
  public:
    typedef gctools::gcbitvector    ImplType;
    typedef gctools::gcbitvector::value_type BitBlockType;
    static const cl_index BitWidth = sizeof(BitBlockType)*8;
  public:
    size_t               _Dimension;
    T_sp                 _FillPointer;
    gctools::gcbitvector _Bits;
    BitVector_sp         _DisplacedTo;
    cl_index             _DisplacedIndexOffset;
  public:
    BitVector_O(size_t dimension, bool init_value = false, T_sp fillPointer=_Nil<T_O>(), BitVector_sp displacedTo = BitVector_sp(), cl_index displacedIndexOffset = 0 );
    constexpr cl_index BitVectorWords(size_t dimension) const { return (dimension+(sizeof(BitBlockType)*8-1)) / sizeof(BitBlockType); };
    BitVector_O(const BitVector_O& other) {
      this->_Dimension = other._Dimension;
      this->_FillPointer = other._FillPointer;
      this->_DisplacedTo = other._DisplacedTo;
      this->_DisplacedIndexOffset = other._DisplacedIndexOffset;
      if (this->_DisplacedTo.nilp()) {
        printf("%s:%d In copy constructor I need to copy the bits from other into my _Bits",__FILE__,__LINE__);
        abort();
      }
    }
  public:
    virtual T_sp type_symbol() const { return cl::_sym_BitVector_O; };
    virtual bool displacedp() const { return !!this->_DisplacedTo; };
    virtual bool adjustableArrayP() const { return true; };
  public:
    virtual T_sp deepCopy() const;
    virtual cl_index dimension() const { return this->_Dimension; };
    virtual T_sp displaced_to() const {
      if (this->_DisplacedTo) return this->_DisplacedTo;
      return _Nil<T_O>();
    }
    virtual cl_index displaced_index_offset() const {
      return this->_DisplacedIndexOffset;
    }

    virtual cl_index length() const { return this->_FillPointer.fixnump() ? this->_FillPointer.unsafe_fixnum() : this->dimension(); };
    void fillPointerSet(T_sp fp);

#if 0
  // This is a bad idea for displaced bit-arrays because they aren't necessarily byte aligned.
    unsigned char *bytes() const { return reinterpret_cast<unsigned char *>(const_cast<BitBlockType*>(this->bits.data())); };
#endif

    void getOnIndices(vector<cl_index> &vals);
    void setOnIndices(const vector<cl_index> &indices);

    bool equal(T_sp bv) const;

    void setBit(cl_index i, uint v);
    uint testBit(cl_index i) const;
    void erase();

    T_sp bit_vector_aset_unsafe(cl_index j, T_sp val) { this->setBit(j,clasp_to_fixnum(val)); return val; };
    T_sp bit_vector_aref_unsafe(cl_index index) const { return clasp_make_fixnum(this->testBit(index)); };

    virtual T_sp aset_unsafe(cl_index j, T_sp val) { return this->bit_vector_aset_unsafe(j,val); };
    virtual T_sp aref_unsafe(cl_index index) const { return this->bit_vector_aref_unsafe(index); };
  
    virtual T_sp svref(cl_index index) const { return this->bit_vector_aref_unsafe(index); };
    virtual T_sp setf_svref(cl_index index, T_sp value) {return this->bit_vector_aset_unsafe(index,value);};

    void sxhash_(HashGenerator &hg) const;
    cl_index lowestIndex();

  //! Calculate the "or" of bv with this BitVector
    void inPlaceOr(BitVector_sp bv);
  //! Calculate the "and" of bv with this BitVector
    void inPlaceAnd(BitVector_sp bv);
  //! Calculate the "xor" of bv with this BitVector
    void inPlaceXor(BitVector_sp bv);

  //! Return a BitVector "or"ed with this
    BitVector_sp bitOr(BitVector_sp bv);
  //! Return a BitVector "and"ed with this
    BitVector_sp bitAnd(BitVector_sp bv);
  //! Return a BitVector "xor"ed with this
    BitVector_sp bitXor(BitVector_sp bv);

  //! Return the number of set bits
    cl_index countSet();
  //! Return true if the BitVector contains only 0's
    CL_LISPIFY_NAME("core:isZero");
    CL_DEFMETHOD   bool isZero() { return (this->countSet() == 0); };

    void do_subseq(BitVector_sp result, cl_index start, cl_index iend) const;

    string asString();

    std::ostream &dumpToStream(std::ostream &out);
    void dump();


    virtual void rowMajorAset(cl_index idx, T_sp value);
    virtual T_sp rowMajorAref(cl_index idx) const;

    virtual T_sp elementType() const { return cl::_sym_bit; };

  public:
    void adjust(bool init_bit, size_t dimension );
    virtual T_sp vectorPush(T_sp newElement);
    virtual Fixnum_sp vectorPushExtend(T_sp newElement, cl_index extension);

  public:
    bool bitVectorP() const { return true; };
    virtual cl_index offset() const { return 0; }; // displaced arrays?  used in bits.cc
    virtual void __write__(T_sp strm) const;

  };

  BitVector_sp make_bit_vector(bool init_value,
                               size_t dimension,
                               bool adjustable = false,
                               T_sp fill_pointer = _Nil<T_O>(),
                               T_sp displaced_to = _Nil<T_O>(),
                               cl_index displaced_index_offset=0);

#if 0
    SMART(SimpleBitVector);
    class SimpleBitVector_O : public BitVector_O {
    LISP_CLASS(core, ClPkg, SimpleBitVector_O, "simple-bit-vector",BitVector_O);

  public:
    static SimpleBitVector_sp make(size_t size,int init_bit=0);

  private:
    size_t _length;

  public:
    virtual cl_index dimension() const { return this->_length; };
    virtual T_sp subseq(cl_index start, T_sp end) const;
    INHERIT_SEQUENCE virtual T_sp elt(cl_index index) const { return this->bit_vector_aref_unsafe(index); };
    INHERIT_SEQUENCE virtual T_sp setf_elt(cl_index index, T_sp value) { return this->bit_vector_aset_unsafe(index,value); };
    T_sp deepCopy() const;
    explicit SimpleBitVector_O(size_t sz, int value) : BitVector_O(sz,value), _length(sz){};
//  explicit SimpleBitVector_O() : BitVector_O(){};
    virtual ~SimpleBitVector_O(){};
  };
#endif
 
#if 0
    SMART(BitVectorWithFillPtr);
    class BitVectorWithFillPtr_O : public BitVector_O {
    LISP_CLASS(core, ClPkg, BitVectorWithFillPtr_O, "bit-vector-with-fill-ptr",BitVector_O);

  public:
    static BitVectorWithFillPtr_sp make(size_t size, size_t fill_ptr, bool adjustable,int init_bit=0);

  private:
    size_t   _Dimension;
    T_sp     _FillPointer;
  
    public:
    virtual cl_index dimension() const { return this->_Dimension; };

    virtual T_sp vectorPush(T_sp newElement);
    virtual Fixnum_sp vectorPushExtend(T_sp newElement, cl_index extension = 8);
    virtual T_sp subseq(cl_index start, T_sp end) const;
    void fillPointerSet(T_sp fp);
    T_sp replace_array(T_sp other) {
      *this = *gc::As<BitVectorWithFillPtr_sp>(other);
      return this->asSmartPtr();
    }
    T_sp deepCopy() const;
    explicit BitVectorWithFillPtr_O(size_t sz, size_t fill_ptr, bool adjust, int value) : BitVector_O(sz,value), _fill_ptr(fill_ptr), _adjustable(adjust){};
//  explicit BitVectorWithFillPtr_O() : BitVector_O(){};
    virtual ~BitVectorWithFillPtr_O(){};
    };
#endif
  };
#endif
