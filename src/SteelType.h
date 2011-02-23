#ifndef SR_STEELTYPE_H
#define SR_STEELTYPE_H

#include <queue>
#include <memory>
#include <tr1/memory>

class SteelFunctor;
using std::tr1::shared_ptr;

class SteelType
{
public:
    SteelType();
    SteelType(const SteelType &);
    ~SteelType();
   
    // The only need for this class is so that dynamic_cast can be used
    class IHandle{
    public:
	IHandle(){}
	virtual ~IHandle(){}
    };

    typedef IHandle * Handle;
    typedef shared_ptr<SteelFunctor> Functor;

    operator int () const;
    operator unsigned int() const { return static_cast<unsigned int>( (int)(*this) ); }
    operator double () const;
    operator std::string () const;
    operator bool () const;
    operator std::vector<SteelType> () const;
    operator Handle () const;

    void set(int i);
    void set(double d);
    void set(bool b);
    void set(const std::string &);
    void set(const std::vector<SteelType> &);
    void set(Handle h);
    void set(Functor f);
   

    // Array stuff
    bool isArray() const { return m_storage == ARRAY; }
    SteelType getElement(int index) const;
    SteelType *getLValue(int index) const;
    void setElement(int index,const SteelType &);
    int getArraySize()const;
    void add(const SteelType &var);
    void removeTail();
    void reserveArray(int index);
    SteelType pop();

    // Handle stuff
    bool isHandle() const { return m_storage == HANDLE; }
    bool isValidHandle() const { return isHandle() && m_value.h != NULL; }

    // Functor stuff
    bool isFunctor() const { return m_storage == FUNCTOR; }
    Functor getFunctor()const;
    // Assignment
    SteelType & operator=(const SteelType &rhs);
    // Unary operators
    SteelType operator-();
    SteelType operator!();
    SteelType operator++();
    SteelType operator++(int); //postfix
    SteelType operator--();
    SteelType operator--(int); //postfix
  
    // Binary operators
    SteelType  operator+(const SteelType &rhs);
    SteelType  operator-(const SteelType &rhs);
    SteelType  operator*(const SteelType &rhs);
    SteelType  operator^(const SteelType &rhs);
    SteelType  operator/(const SteelType &rhs);
    SteelType  operator%(const SteelType &rhs);
    SteelType  operator<(const SteelType &rhs);
    SteelType  operator<=(const SteelType &rhs);
    SteelType  operator>(const SteelType &rhs);
    SteelType  operator>=(const SteelType &rhs);
    SteelType  d(const SteelType &rhs);
    // Assignment binary ops
    SteelType operator+=(const SteelType &rhs);
    SteelType operator-=(const SteelType &rhs);
    SteelType operator*=(const SteelType &rhs);
    SteelType operator/=(const SteelType &rhs);
    SteelType operator%=(const SteelType &rhs);

    bool isConst()const;
    void makeConst();
private:
    enum storage
    {
        ARRAY,
        BOOL,
        INT,
        DOUBLE,
        STRING,
        HANDLE,
	FUNCTOR
    };

    int strInt() const ;
    double strDouble()const;
    bool strBool()const;
    std::string strToBool(bool b)const;
    std::string strToInt(int i)const;
    std::string strToDouble(double d)const;

    
    union value
    {
        bool b;
        double d;
        int i;
        Handle h;
        std::string *s;
        std::vector<SteelType> *a;
    };
    Functor m_functor;
    value m_value;
    storage m_storage;
    bool m_bConst;
    friend bool operator==(const SteelType &lhs, const SteelType &rhs);
};


typedef std::vector<SteelType> SteelArray;

bool operator==(const SteelType &lhs, const SteelType &rhs);
bool operator!=(const SteelType &lhs, const SteelType &rhs);
#endif


