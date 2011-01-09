#include <string>
#include <sstream>
#include <cmath>
#include "SteelType.h"
#include "SteelException.h"
#include <cassert>
#include <iomanip>
#include <stdlib.h>

SteelType::SteelType()
{
    m_value.i = 0;
    m_value.s = NULL;
    m_value.a = NULL;
    m_value.h = NULL;
    m_storage = SteelType::INT;
    m_bConst = false;
}

SteelType::SteelType(const SteelType &rhs)
{
    set(0);
    *this = rhs;
    m_bConst = false;
}

SteelType::~SteelType()
{
    if(m_storage == SteelType::STRING)
        delete m_value.s;
    else if (m_storage == SteelType::ARRAY)
        delete m_value.a;

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
    case SteelType::HANDLE:
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
    case SteelType::HANDLE:
        throw TypeMismatch();
    }
    assert( 0 );
    return 0.0;
}

SteelType::operator std::string () const
{
    switch(m_storage)
    {
    case SteelType::ARRAY:
        throw TypeMismatch();
    case SteelType::BOOL:
        if(m_value.b) return "TRUE";
        else return "FALSE";
    case SteelType::HANDLE:
        // Intentional fallthrough.. kind of hacky
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
    case SteelType::ARRAY:
        return true;
    case SteelType::HANDLE:
        return (m_value.h == NULL?false:true);
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

SteelType::operator std::vector<SteelType> () const
{
    if( !isArray() ) throw TypeMismatch();

    else return *m_value.a;
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


void SteelType::set(const std::vector<SteelType> &ref)
{
    m_value.a = new std::vector<SteelType>(ref);
    m_storage = SteelType::ARRAY;
}

SteelType & SteelType::operator=(const SteelType &rhs)
{
    if(&rhs == this) return *this;

    if(m_storage == SteelType::STRING)
        delete m_value.s;
    else if (m_storage == SteelType::ARRAY)
        delete m_value.a;

    m_storage = SteelType::INT;

    if(rhs.m_storage == SteelType::STRING)
        set ( *rhs.m_value.s );
    else if ( rhs.m_storage == SteelType::ARRAY)
        set ( *rhs.m_value.a );
    else if ( rhs.m_storage == SteelType::BOOL)
        set ( rhs.m_value.b );
    else if ( rhs.m_storage == SteelType::INT)
        set ( rhs.m_value.i );
    else if ( rhs.m_storage == SteelType::DOUBLE)
        set ( rhs.m_value.d );
    else if (rhs.m_storage == SteelType::HANDLE)
        set( rhs.m_value.h );

    return *this;
}


SteelType SteelType::pop()
{
    if( ! isArray() ) throw TypeMismatch();

    SteelType back = (*m_value.a).back();
    (*m_value.a).pop_back();
    return back;
}

SteelType  SteelType::operator+(const SteelType &rhs)
{
    SteelType val;
    storage s = std::max(m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
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
    }

    assert ( 0 );
    return val;
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

bool  operator==(const SteelType &lhs, const SteelType &rhs)
{
    bool val = false;
    SteelType::storage s = std::max(lhs.m_storage,rhs.m_storage);

    switch(s)
    {
    case SteelType::ARRAY:
        if( rhs.m_storage == SteelType::ARRAY
            && lhs.m_storage == SteelType::ARRAY
            && *lhs.m_value.a == *rhs.m_value.a )
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
        if((void*)lhs == (void*)rhs)
            val = true;
        break;
       
    }

    return val;
}

bool  operator!=(const SteelType &lhs, const SteelType &rhs)
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
        throw OperationMismatch();
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
        throw OperationMismatch();
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
    case SteelType::ARRAY:
        throw OperationMismatch();
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

    return (*m_value.a)[index];
}

SteelType *SteelType::getLValue(int index) const
{
    // Not a valid lvalue if its const
    // TODO: Throw ConstViolation. But then you have to catch it
    if(m_bConst) return NULL;

    if ( index >= getArraySize() ) throw OutOfBounds();

    return & ((*m_value.a)[index]);
}

void SteelType::setElement(int index,const SteelType &val)
{
    // I would check that we're an array, but getArraySize does already

    if( index >= getArraySize() ) throw OutOfBounds();

    (*m_value.a)[index] = val;
}

int SteelType::getArraySize()const
{
    if( !isArray() ) throw TypeMismatch();

    int val = m_value.a->size();
    return val;
}

void SteelType::add(const SteelType &var)
{
    if( !isArray() ) throw TypeMismatch();

    m_value.a->push_back ( var );
}


void SteelType::removeTail()
{
    if(!isArray()) throw TypeMismatch();

    m_value.a->pop_back();
}


void SteelType::reserveArray(int index)
{
    if ( ! isArray() ) throw TypeMismatch();

    m_value.a->reserve ( index );
}


bool SteelType::isConst()const
{
    return m_bConst;
}

void SteelType::makeConst()
{
    m_bConst=true;
}

