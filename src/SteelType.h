#ifndef SR_STEELTYPE_H
#define SR_STEELTYPE_H

#include <queue>


class SteelType
{
private:
    enum storage
    {
        ARRAY,
        BOOL,
        INT,
        DOUBLE,
        STRING,
        HANDLE
    };

public:
    SteelType();
    SteelType(const SteelType &);
    ~SteelType();

    typedef void * Handle;

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
    void set(Handle p);

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
    SteelType cat(const SteelType &rhs);

    bool isConst()const;
    void makeConst();
private:
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
        void *h;
        std::string *s;
        std::vector<SteelType> *a;
    };
    value m_value;
    storage m_storage;
    bool m_bConst;
    friend bool operator==(const SteelType &lhs, const SteelType &rhs);
};


typedef std::vector<SteelType> SteelArray;

bool operator==(const SteelType &lhs, const SteelType &rhs);
bool operator!=(const SteelType &lhs, const SteelType &rhs);
#endif


