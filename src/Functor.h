
#ifndef STEEL_FUNCTOR_H
#define STEEL_FUNCTOR_H

#include <string>
#include <queue>

class SteelType
{
public:
    SteelType();
    SteelType(const SteelType &);
    virtual ~SteelType();

    operator int ();
    operator double ();
    operator std::string ();
    operator bool ();

    SteelType & operator=(const SteelType &rhs);

private:
};

class ParamList
{
public:
    ParamList();
    virtual ~ParamList();
    
    SteelType next();
    void enqueue(const SteelType &type);
private:
    std::queue<SteelType> m_params;
};

class SteelFunctor
{
public:
    SteelFunctor();
    virtual ~SteelFunctor();

    virtual SteelType Call(const ParamList &params)=0;

private:
};

template<typename ObjType>
class SteelFunctorNoArgs : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)();
    SteelFunctorNoArgs(ObjType *pObj, FuncPointer p);
    virtual ~SteelFunctorNoArgs();
    virtual SteelType Call(const ParamList &params);
private:
    FuncPointer *m_pFunc;
    ObjType *m_pObj;
};

template<typename ObjType, typename Arg1>
    class SteelFunctor1Arg : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)(Arg1);
    virtual SteelType Call(const ParamList &params);
    SteelFunctor1Arg(ObjType *pObj, FuncPointer p);
       
private:
    FuncPointer *m_pFunc;
    ObjType *m_pObj;
};

template<typename ObjType, typename Arg1, typename Arg2>
    class SteelFunctor2Arg : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)(Arg1,Arg2);
    virtual SteelType Call(const ParamList &params);
    SteelFunctor2Arg(ObjType *pObj, FuncPointer p);
private:
    FuncPointer * m_pFunc;
    ObjType * m_pObj;
};

/*

template <typename Class>
class Thunk
{
public:
    typedef SteelType (Class::*MemberFunction)() ;
    Thunk(ParamList *pParams,Class *pObject, SteelFunctor f);
    virtual ~Thunk();

private:
};

*/
#endif


