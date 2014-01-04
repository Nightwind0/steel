#include <string>
#include <sstream>
#include <cmath>
#include "SteelType.h"
#include "SteelException.h"
#include <cassert>
#include <iomanip>
#include <stdlib.h>
#include <algorithm>
#include <iostream>

using namespace Steel;

SteelType::SteelType()
{
    m_value.i = 0;
    m_storage = SteelType::INT;
    m_bConst = false;
    m_bCopyArray = m_bCopyHash = false;
}

SteelType::SteelType(const SteelType &rhs)
{
    set(0);
    *this = rhs;
    m_bConst = false;
}

SteelType::SteelType(SteelType&& rhs):m_functor(std::move(rhs.m_functor)),m_array(std::move(rhs.m_array)),m_map(std::move(rhs.m_map)),m_value(rhs.m_value),m_storage(rhs.m_storage),m_bConst(rhs.m_bConst),m_bCopyArray(rhs.m_bCopyArray),m_bCopyHash(rhs.m_bCopyHash)
{
    rhs.m_value.i = 0;
    rhs.m_storage = SteelType::INT;
}


SteelType::~SteelType()
{
    if(m_storage == SteelType::STRING)
        delete m_value.s;
     m_storage = SteelType::INT;
}


SteelType::operator int () const
{
    switch(m_storage)
    {
    case SteelType::ARRAY:
        throw TypeMismatch();
    case SteelType::BOOL:
        if(m_value.b) return 1;
        else return 0;
    case SteelType::INT:
        return m_value.i;
    case SteelType::DOUBLE:
        return static_cast<int>(m_value.d);
    case SteelType::STRING:
        return strInt();
    case SteelType::FUNCTOR:
    case SteelType::HANDLE:
    case SteelType::HASHMAP:
        throw TypeMismatch();
    }
    assert ( 0 );
    return 0;
}

SteelType::operator Handle() const
{
    if(m_storage != SteelType::HANDLE)
        throw TypeMismatch();
    else return m_value.h;
}

SteelType::operator Functor() const
{
    if(m_storage != SteelType::FUNCTOR)
        throw TypeMismatch();
    else return m_functor;
}

SteelType::operator double () const
{
    switch(m_storage)
    {
    case SteelType::ARRAY:
        throw TypeMismatch();
    case SteelType::BOOL:
        if(m_value.b) return 1.0;
        else  return 0.0;
    case SteelType::INT:
        return m_value.i;
    case SteelType::DOUBLE:
        return m_value.d;
    case SteelType::STRING:
        return strDouble();
    case SteelType::FUNCTOR:
    case SteelType::HANDLE:
    case SteelType::HASHMAP:
        throw TypeMismatch();
    }
    assert( 0 );
    return 0.0;
}

SteelType::operator std::string () const
{
    switch(m_storage)
    {
    case SteelType::BOOL:
        if(m_value.b) return "TRUE";
        else return "FALSE";
    case SteelType::FUNCTOR:{
        std::ostringstream os;
        os << "#F(" << std::hex << m_functor.get() << ')';
        return os.str();
    }
    case SteelType::ARRAY:	{
        std::ostringstream os;
        os << "#A(" << std::hex << m_array.get() << ')';
        return os.str();
    }
    case SteelType::HANDLE:{
        std::ostringstream os;
        os << "#H(" << std::hex << m_map.get() << ')';
        return os.str();
    }
    case SteelType::HASHMAP:{
        std::ostringstream os;
        os << "#M(" << std::hex << m_array.get() << ')';
        return os.str();
    }
    case SteelType::INT:
        return strToInt(m_value.i);
    case SteelType::DOUBLE:
        return strToDouble(m_value.d);
    case SteelType::STRING:
        return *m_value.s;
    }
    return "";
}


SteelType::operator bool () const
{
    switch(m_storage)
    {
    case SteelType::FUNCTOR:
        return m_functor != NULL;
    case SteelType::HASHMAP:
    case SteelType::ARRAY:
        return true;
    case SteelType::HANDLE:
        return (m_map == NULL?false:true);
    case SteelType::BOOL:
        return m_value.b;
    case SteelType::INT:
        return m_value.i != 0;
    case SteelType::DOUBLE:
        return m_value.d != 0.0;
    case SteelType::STRING:
        return !m_value.s->empty();
    }
    return true;
}

SteelType::operator SteelType::Container () const
{
    if( !isArray() ) throw TypeMismatch();

    else return *m_array;
}

void SteelType::set(int i)
{
    m_value.i = i;
    m_storage = SteelType::INT;
}

void SteelType::set(double d)
{
    m_value.d = d;
    m_storage = SteelType::DOUBLE;
}

void SteelType::set(bool b)
{
    m_value.b = b;
    m_storage = SteelType::BOOL;
}

void SteelType::set(const std::string &str)
{
    m_value.s  = new std::string(str);
    m_storage = SteelType::STRING;
}

void SteelType::set(Handle h)
{
    m_value.h = h;
    m_storage = SteelType::HANDLE;
}

void SteelType::set(Functor f)
{
    m_functor = f;
    m_value.i = 0;
    m_storage = SteelType::FUNCTOR;
}


void SteelType::set(const Container &ref)
{
    m_array = std::shared_ptr<SteelType::Container>(new SteelType::Container(ref));
    m_storage = SteelType::ARRAY;
    m_bCopyArray = false;
}

void SteelType::set(const Map &ref)
{
    m_map = std::shared_ptr<SteelType::Map>(new SteelType::Map(ref));
    m_storage = SteelType::HASHMAP;
    m_bCopyHash = false;
}

SteelType & SteelType::operator=(SteelType&& rhs){
  m_functor = std::move(rhs.m_functor);
  m_array = std::move(rhs.m_array);
  m_map = std::move(rhs.m_map);
  m_value = rhs.m_value;
  m_storage = rhs.m_storage;
  m_bCopyArray = rhs.m_bCopyArray; // Right? Move this value?
  m_bCopyHash = rhs.m_bCopyHash; // Is it correct to move this? I think so
  rhs.m_value.i = 0;
  rhs.m_storage = SteelType::INT;
  return *this;
}

SteelType & SteelType::operator=(const SteelType &rhs)
{
    if(&rhs == this) return *this;

    if(m_storage == SteelType::STRING)
        delete m_value.s;

    m_storage = SteelType::INT;

    if(rhs.m_storage == SteelType::STRING)
        set ( *rhs.m_value.s );
    else if ( rhs.m_storage == SteelType::ARRAY){
        //set ( *rhs.m_value.a );
        m_bCopyArray = true;
        m_array = rhs.m_array;
        m_storage = SteelType::ARRAY;
    }else if ( rhs.m_storage == SteelType::BOOL)
        set ( rhs.m_value.b );
    else if ( rhs.m_storage == SteelType::INT)
        set ( rhs.m_value.i );
    else if ( rhs.m_storage == SteelType::DOUBLE)
        set ( rhs.m_value.d );
    else if ( rhs.m_storage == SteelType::HANDLE)
        set ( rhs.m_value.h );
    else if ( rhs.m_storage == SteelType::FUNCTOR)
        set ( rhs.m_functor );
    else if ( rhs.m_storage == SteelType::HASHMAP){
        //set ( *rhs.m_value.m );
        m_bCopyHash = true;
        m_map = rhs.m_map;
        m_storage = SteelType::HASHMAP;
    }

    return *this;
}

SteelType::Functor SteelType::getFunctor() const 
{ 
    if(!isFunctor())
        throw ValueNotFunction();
    return m_functor; 
}

SteelType SteelType::pop_back()
{
    if( ! isArray() ) throw TypeMismatch();

    if(m_bCopyArray){
        set(*m_array);
        m_bCopyArray = false;
    }

    SteelType back = m_array->back();
    m_array->pop_back();
    return back;
}

SteelType SteelType::pop()
{
    if( ! isArray() ) throw TypeMismatch();

    if(m_bCopyArray){
        set(*m_array);
        m_bCopyArray = false;
    }
    
    SteelType front = m_array->front();
    m_array->pop_front();
    return front;
}

SteelType SteelType::removeElement ( int index )
{
    if( ! isArray() ) throw TypeMismatch();

    if(m_bCopyArray){
        set(*m_array);
        m_bCopyArray = false;
    }
    
    SteelType val = m_array->operator[](index);
    m_array->erase(m_array->begin() + index);
    return val;
}

SteelType* SteelType::getLValue( const std::string& key ) const 
{
    if ( !isHashMap() ) throw TypeMismatch();
    
    if(m_bCopyHash){
        const_cast<SteelType*>(this)->set(*m_map); // Copy-on-write. getting an L-value is a write.
        const_cast<SteelType*>(this)->m_bCopyHash = false;
    }

    Map::iterator findit = m_map->find(key);
    if(findit == m_map->end()) {
        // Throw exception... or silent but graceful failure?
        //throw UnknownIdentifier();
        m_map->operator[](key) = SteelType();
        return &(m_map->operator[](key));
    }
    return &(findit->second);
}


SteelType SteelType::getElement( const std::string& key ) const 
{
    if ( !isHashMap() ) throw TypeMismatch();
    return m_map->operator[](key);
}

void SteelType::setElement( const std::string& key, const SteelType& value ) 
{
    if ( !isHashMap() ) throw TypeMismatch();
    if(m_bCopyHash){
        set(*m_map);
        m_bCopyHash = false;
    }
    (*m_map)[key] = value;
}


void SteelType::shuffle () 
{
    if( ! isArray() ) throw TypeMismatch();
    
    std::random_shuffle(m_array->begin(),m_array->end());
}


void SteelType::push(const SteelType &var) // adds to the front
{
    if (! isArray() ) throw TypeMismatch();

    m_array->push_front(var);
}


SteelType SteelType::operator+=(const SteelType &rhs)
{
    storage s = std::max(m_storage,rhs.m_storage);
    
    
    if(m_storage == SteelType::ARRAY)
    {
        if(m_bCopyArray){
            set(*m_array); // Copy on write
            m_bCopyArray = false;
        }
        add(rhs);
        return *this;
    }
    else if(rhs.m_storage == SteelType::ARRAY)
    {
        if(m_bCopyArray){
            set(*m_array); // Copy on write
            m_bCopyArray = false;
        }
        SteelType val = *this;
        set(Container());
        add(val);
        add(rhs);
        return *this;
    }
    
    if(m_storage == SteelType::HASHMAP){
        if(rhs.m_storage == SteelType::HASHMAP){
            if(m_bCopyHash){
                set(*m_map); // Copy on write
                m_bCopyHash = false;
            }
            add(*rhs.m_map);
            return *this;
        }else{
            throw OperationMismatch();
        }
    }

    switch(s)
    {
    case SteelType::FUNCTOR:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::HASHMAP:
        throw OperationMismatch();
    case SteelType::INT:
        set ( (int)*this + (int)rhs );
        break;
    case SteelType::DOUBLE:
        set ( (double)*this + (double)rhs );
        break;
    case SteelType::STRING:
        set ( (std::string)*this + (std::string)rhs );
        break;
    }

    return *this;
}

SteelType SteelType::operator-=(const SteelType &rhs)
{
    storage s = std::max(m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::STRING:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:
        throw OperationMismatch();
    case SteelType::INT:
        set ( (int)*this - (int)rhs );
        break;
    case SteelType::DOUBLE:
        set ( (double)*this - (double)rhs );
        break;
    case SteelType::HASHMAP:
        if(m_bCopyHash){
            set(*m_map); // Copy on write
            m_bCopyHash = false;
        }
        m_map->erase((std::string)rhs);
        break;
    }

    return *this;
}

SteelType SteelType::operator*=(const SteelType &rhs)
{
    storage s = std::max(m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::STRING:
    case SteelType::FUNCTOR:
    case SteelType::HASHMAP:
        throw OperationMismatch();
    case SteelType::INT:
        set ( (int)*this * (int)rhs );
        break;
    case SteelType::DOUBLE:
        set ( (double)*this * (double)rhs );
        break;
    }

    return *this;

}

SteelType SteelType::operator/=(const SteelType &rhs)
{
    storage s = std::max(m_storage,rhs.m_storage);

    double right = (double)rhs;
    if(right == 0.0) throw DivideByZero();
    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::STRING:
    case SteelType::FUNCTOR:
    case SteelType::HASHMAP:
        throw OperationMismatch();
    case SteelType::INT:
        set ( (int)*this / (int)rhs );
        break;
    case SteelType::DOUBLE:
        set ( (double)*this / (double)rhs );
        break;

    }

    return *this;

}

SteelType SteelType::operator%=(const SteelType &rhs)
{
    storage s = std::max(m_storage,rhs.m_storage);

    if((double)rhs == 0.0) throw DivideByZero();

    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::STRING:
    case SteelType::FUNCTOR:
    case SteelType::HASHMAP:
        throw OperationMismatch();
    case SteelType::INT:
        set ( (int)*this % (int)rhs );
        break;
    case SteelType::DOUBLE:
        set ( (int)*this % (int)rhs );
        break;

    }

    return *this;
}

SteelType  SteelType::operator+(const SteelType &rhs)
{
    SteelType val;
    storage s = std::max(m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
        if(m_storage == SteelType::ARRAY)
	{
            val = *this;
            val.add(rhs);
	}
        else
	{
            val.set(Container());
            val.add(*this);
            val.add(rhs);
	}
        return val;
    case SteelType::FUNCTOR:
    case SteelType::BOOL:
    case SteelType::HANDLE:
        throw OperationMismatch();
        return val;
    case SteelType::INT:
        val.set ( (int)*this + (int)rhs );
        return val;
    case SteelType::DOUBLE:
        val.set ( (double)*this + (double)rhs);
        return val;
    case SteelType::STRING:
        val.set ( (std::string)*this + (std::string)rhs);
        return val;
    case SteelType::HASHMAP:{
        if(m_storage == SteelType::HASHMAP && rhs.m_storage == HASHMAP){
            val = *this;
            val.add(*rhs.m_map); // This will cause a C-O-W
            return val;
        }else{
            throw OperationMismatch();
        }
    }
        return val;		
    }
    

    assert ( 0 );
    return val;
}


void SteelType::add(const SteelType::Map& i_map)
{
    if(!isHashMap()) throw OperationMismatch();
    for(SteelType::Map::const_iterator it = i_map.begin(); it != i_map.end(); it++){
        //m_value.m->insert(SteelType::Map::value_type(it->first,it->second));
        // TODO: Overwrite existing values?? Add them with operator+??? ignore them??
        (*m_map)[it->first] = it->second;
    }
}

SteelType  SteelType::operator-(const SteelType &rhs)
{
    SteelType val;
    storage s = std::max(m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:
        throw OperationMismatch();
        return val;
    case SteelType::INT:
        val.set ( (int)*this - (int)rhs );
        return val;
    case SteelType::DOUBLE:
        val.set ( (double)*this - (double)rhs);
        return val;
    case SteelType::STRING:
        throw OperationMismatch();
        return val;
    case SteelType::HASHMAP:
        val.set(*m_map);
        val -= rhs; // in order to activate val's copy-on-write
        //val.m_map->erase((std::string)rhs);
        return val;
    }

    assert ( 0 );
    return val;
}

SteelType  SteelType::operator*(const SteelType &rhs)
{
    SteelType val;
    storage s = std::max(m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:
    case SteelType::HASHMAP:
        throw OperationMismatch();
        return val;
    case SteelType::INT:
        val.set ( (int)*this * (int)rhs );
        return val;
    case SteelType::DOUBLE:
        val.set ( (double)*this * (double)rhs);
        return val;
    case SteelType::STRING:
        throw OperationMismatch();
        return val;
    }

    assert ( 0 );
    return val;
}

SteelType  SteelType::operator^(const SteelType &rhs)
{
    SteelType val;
    storage s = std::max(m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:
    case SteelType::HASHMAP:
        throw OperationMismatch();
        return val;
    case SteelType::INT:
        val.set ( pow(*this,(int)rhs) );
        return val;
    case SteelType::DOUBLE:
        val.set ( pow(*this,(double)rhs) );
        return val;
    case SteelType::STRING:
        throw OperationMismatch();
        return val;
    }

    assert ( 0 );
    return val;
}

SteelType  SteelType::operator/(const SteelType &rhs)
{
    SteelType val;
    storage s = std::max(m_storage,rhs.m_storage);

    double right = (double)rhs;

    if(right == 0.0) throw DivideByZero();

    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:
    case SteelType::HASHMAP:
        throw OperationMismatch();
        return val;
    case SteelType::INT:
        val.set ( (int)*this / right );
        return val;
    case SteelType::DOUBLE:
        val.set ( (double)*this / right);
        return val;
    case SteelType::STRING:
        throw OperationMismatch();
        return val;
    }

    assert ( 0 );
    return val;
}

SteelType  SteelType::operator%(const SteelType &rhs)
{
    SteelType val;
    storage s = std::max(m_storage,rhs.m_storage);

    int right = (int)rhs;

    if(right == 0) throw DivideByZero();

    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:
    case SteelType::HASHMAP:
        throw OperationMismatch();
        return val;
    case SteelType::INT:
        val.set ( (int)*this % right );
        return val;
    case SteelType::DOUBLE:
        val.set ( (int)*this % right );
        return val;
    case SteelType::STRING:
        throw OperationMismatch();
        return val;
    }

    assert ( 0 );
    return val;
}

bool  Steel::operator==(const SteelType &lhs, const SteelType &rhs)
{
    bool val = false;
    SteelType::storage s = std::max(lhs.m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
        if( rhs.m_storage == SteelType::ARRAY
            && lhs.m_storage == SteelType::ARRAY
            && *lhs.m_array == *rhs.m_array )
            val = true;
        break;
    case SteelType::BOOL:
        if((bool)lhs == (bool)rhs)
            val = true;
        break;
    case SteelType::INT:
        if((int)lhs == (int)rhs)
            val = true;
        break;
    case SteelType::DOUBLE:
        if((double)lhs == (double)rhs)
            val = true;
        break;
    case SteelType::STRING:
        if((std::string)lhs == (std::string)rhs)
            val = true;
        break;
    case SteelType::HANDLE:
        if((SteelType::IHandle*)lhs == (SteelType::IHandle*)rhs)
            val = true;
        break;
    case SteelType::FUNCTOR:
        if( rhs.m_storage == SteelType::FUNCTOR &&
            lhs.m_storage == SteelType::FUNCTOR &&
            rhs.m_functor == lhs.m_functor)
            val= true;
 
        break;
    case SteelType::HASHMAP:
        if(rhs.m_storage == SteelType::HASHMAP &&
           lhs.m_storage == SteelType::HASHMAP && 
           *rhs.m_map == *lhs.m_map)
            val = true;
        break;
    }
	
    return val;
}

bool  Steel::operator!=(const SteelType &lhs, const SteelType &rhs)
{
    return ! (lhs == rhs );
}

SteelType  SteelType::operator<(const SteelType &rhs)
{
    SteelType val;
    val.set(false);
    storage s = std::max(m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:{
		
        val.set( (std::string)*this < (std::string)rhs );
    }
        break;
    case SteelType::INT:
        if((int)*this < (int)rhs)
            val.set(true);
        break;
    case SteelType::DOUBLE:
        if((double)*this < (double)rhs)
            val.set(true);
        break;
    case SteelType::STRING:
        if((std::string)*this < (std::string)rhs)
            val.set(true);
        break;
       
    }

    return val;
}

SteelType  SteelType::operator<=(const SteelType &rhs)
{
    SteelType val;
    val.set(false);
    storage s = std::max(m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:
        val.set( (std::string)*this <= (std::string)rhs);
        break;
    case SteelType::INT:
        if((int)*this <= (int)rhs)
            val.set(true);
        break;
    case SteelType::DOUBLE:
        if((double)*this <= (double)rhs)
            val.set(true);
        break;
    case SteelType::STRING:
        if((std::string)*this <= (std::string)rhs)
            val.set(true);
        break;
       
    }
    
    return val;
}

SteelType  SteelType::operator>(const SteelType &rhs)
{
    SteelType var;
    if( *this <= rhs ) 
        var.set(false);
    else var.set(true);
    
    return var;
}

SteelType  SteelType::operator>=(const SteelType &rhs)
{
    SteelType var;
    
    if(*this < rhs )
        var.set(false);
    else var.set(true);

    return var;
}

SteelType  SteelType::d(const SteelType &rhs)
{
    SteelType var;
    int dice = (int)*this;
    int sides = (int)rhs;

    if(sides == 0){
        throw DivideByZero();
    }

    int total = 0;

    for(int d = 0; d < dice; d++)
    {
        total +=  rand() % sides + 1;
    }

    var.set(total);

    return var;
}




SteelType SteelType::operator-()
{
    SteelType var;
    switch(m_storage)
    {
    case SteelType::BOOL:
    case SteelType::STRING:
    case SteelType::ARRAY:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:
    case SteelType::HASHMAP:
        throw OperationMismatch();
    case SteelType::INT:
        var.set(0 - m_value.i);
        break;
    case SteelType::DOUBLE:
        var.set(0.0 - m_value.d);
        break;
    }

    return var;
}

SteelType SteelType::operator!()
{
    SteelType var;
    switch(m_storage)
    {
    case SteelType::BOOL:
        if(m_value.b)
            var.set ( false );
        else var.set( true );
        break;
    case SteelType::INT:    
        if(m_value.i == 0)
            var.set(true);
        else var.set ( false );
        break;
    case SteelType::STRING:
        if(m_value.s->empty())
            var.set( true );
        else var.set ( false );
        break;
    case SteelType::DOUBLE:
        if(m_value.d == 0.0)
            var.set( true );
        else var.set ( false );
        break;
    case SteelType::HANDLE:
        if(m_value.h == NULL)
            var.set(true);
        else var.set(false);
        break;
    case SteelType::FUNCTOR:
        var.set(m_functor == NULL);
        break;
    case SteelType::ARRAY:
        var.set(m_array == nullptr);
        break;
    case SteelType::HASHMAP:
        var.set(m_map == NULL);
        break;
    }
    return var;
}

SteelType SteelType::operator++()
{
    switch(m_storage)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::STRING:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:
    case SteelType::HASHMAP:
        throw OperationMismatch();
    case SteelType::INT:
        m_value.i = m_value.i + 1;
        break;
    case SteelType::DOUBLE:
        m_value.d = m_value.d + 1;
        break;
    }

    return *this;
}

SteelType SteelType::operator++(int) //postfix
{
    SteelType result = *this;

    ++(*this);

    return result;
}

SteelType SteelType::operator--()
{
    switch(m_storage)
    {
    case SteelType::ARRAY:
    case SteelType::BOOL:
    case SteelType::STRING:
    case SteelType::HANDLE:
    case SteelType::FUNCTOR:
    case SteelType::HASHMAP:
        throw OperationMismatch();
    case SteelType::INT:
        m_value.i = m_value.i - 1;
        break;
    case SteelType::DOUBLE:
        m_value.d = m_value.d - 1;
        break;
    }

    return *this;
}

SteelType SteelType::operator--(int) //postfix
{
    SteelType result = *this;

    --(*this);

    return result;
}

int SteelType::strInt() const
{
    if(m_storage != SteelType::STRING) return 0;

    std::istringstream str(*m_value.s);

    int i;
    str >> i;

    return i;
}

double SteelType::strDouble() const
{
    if(m_storage != SteelType::STRING) return 0.0;
    std::istringstream str(*m_value.s);
    
    double d;
    str >> d;
    
    return d;
}

bool SteelType::strBool() const
{
    if(m_storage == SteelType::STRING && *m_value.s == "TRUE") return true;
    else return false;
}

std::string SteelType::strToBool(bool b) const
{
    if(b)
        return "TRUE";
    else return "FALSE";
}

std::string SteelType::strToInt(int i) const
{
    std::ostringstream str;

    str << std::setprecision(32);
    str << i;

    return str.str();
}

std::string SteelType::strToDouble(double d) const
{
    std::ostringstream str;

    str << std::setprecision(10);
    str << d;

    return str.str();
}

SteelType SteelType::getElement(int index) const
{
    // I would check that we're an array, but getArraySize does already
    if( index >= getArraySize() ) throw OutOfBounds();

    return (*m_array)[index];
}

SteelType *SteelType::getLValue(int index) const
{
    // Not a valid lvalue if its const
    // TODO: Throw ConstViolation. But then you have to catch it
  if(m_bConst) throw ConstViolation();
    if(m_bCopyArray){
        const_cast<SteelType*>(this)->set(*m_array); // Getting an Lvalue is a write. Copy-on-write
        const_cast<SteelType*>(this)->m_bCopyArray = false;
    }

    if ( index >= getArraySize() ) throw OutOfBounds();

    return & ((*m_array)[index]);
}

void SteelType::setElement(int index,const SteelType &val)
{
    // I would check that we're an array, but getArraySize does already

    if( index >= getArraySize() ) throw OutOfBounds();

    if(m_bCopyArray){
        set(*m_array);
        m_bCopyArray = false;
    }

    (*m_array)[index] = val;
}

int SteelType::getArraySize()const
{
    if( !isArray() ) throw TypeMismatch();

    int val = m_array->size();
    return val;
}

void SteelType::add(const SteelType &var)
{
    if( !isArray() ) throw TypeMismatch();

    if(m_bCopyArray){
        set(*m_array); // Copy-on-write
        m_bCopyArray = false;
    }

    if(var.isArray())
    {
        for(int i=0;i<var.getArraySize();i++)
            m_array->push_back ( var.getElement(i) ); // Don't call add(), you'll flatten any arrays underneath
    }
    else
    {
        m_array->push_back ( var );
    }
}

// Difference between this and add is that it won't flatten an array
void SteelType::pushb(const SteelType& var)
{
    if( !isArray() ) throw TypeMismatch();
    
    if(m_bCopyArray){
        set(*m_array);
        m_bCopyArray = false;
    }
    
    m_array->push_back ( var );
}


bool SteelType::isConst()const
{
    return m_bConst;
}

void SteelType::makeConst()
{
    m_bConst=true;
}

#ifndef NDEBUG
void SteelType::debugPrint()
{
    switch(m_storage){
    case INT:
    case DOUBLE:
    case BOOL:
        std::cout << (std::string)*this << std::endl;
        break;
    case ARRAY:
        std::cout << "Array with " << m_array->size() << std::endl;
        break;
    case HANDLE:
        std::cout << "Handle" << std::endl;
        break;
    case FUNCTOR:
        std::cout << "Functor Count:" << m_functor.use_count() << std::endl;
        break;
    case HASHMAP:
        std::cout << "Hash Map with " << m_map->size() << std::endl;
        break;
    default:
        std::cout << "Bogus storage type" << std::endl;
    }
}
#endif


