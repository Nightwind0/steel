
#ifndef STEEL_FUNCTOR_H
#define STEEL_FUNCTOR_H

#include <string>
#include <deque>
#include <map>
#include "SteelType.h"
#include "SteelException.h"
#include "AuxVariables.h"

namespace Steel { 

// Fowards
class AstParamDefinitionList;
class AstStatementList;
class SteelInterpreter;

 using BoundVariables = std::map<std::string,SteelType>;

class SteelFunctor
{
public:
    SteelFunctor();
    virtual ~SteelFunctor();
    virtual bool isUserFunction() const { return false; }
    virtual bool isSafeFunction() const { return false; } // Is the function OK to run in a safeRun
    virtual SteelType Call(SteelInterpreter *,const SteelType::Container &params)=0;
    virtual std::string getIdentifier() const { return m_identifier;}
    virtual void setIdentifier(const std::string& id) { m_identifier = id; }
private:
    std::string m_identifier;
};

class SteelUserFunction : public SteelFunctor
{
public:
  SteelUserFunction(std::shared_ptr<AstParamDefinitionList>, std::shared_ptr<AstStatementList>);
    virtual ~SteelUserFunction();
    virtual bool isUserFunction() const { return true; }
    virtual SteelType Call(SteelInterpreter * pInterpreter,const SteelType::Container &params);
    void bindNonLocals(SteelInterpreter*); 
private:
    std::shared_ptr<AstParamDefinitionList> m_pParams;
    std::shared_ptr<AstStatementList> m_pList;
    BoundVariables m_bound_variables;
};

 class SteelCBoundFunctor;

template<class ObjType>
class SteelFunctorNoArgs : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)();
    SteelFunctorNoArgs(ObjType *pObj, FuncPointer p):
        m_pFunc(p),m_pObj(pObj){}
        virtual ~SteelFunctorNoArgs(){}
        virtual SteelType Call(SteelInterpreter*,const SteelType::Container &params)
        {
            return (m_pObj->*m_pFunc)();
        }
private:
        FuncPointer m_pFunc;
        ObjType *m_pObj;
};

template<class ObjType, class Arg1>
    class SteelFunctor1Arg : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)(Arg1);
    SteelFunctor1Arg(ObjType *pObj, FuncPointer p)
        :m_pFunc(p),m_pObj(pObj){}
        virtual ~SteelFunctor1Arg(){}
        virtual SteelType Call(SteelInterpreter*,const SteelType::Container &params)
        {
            if(params.size() != 1) throw ParamMismatch();
            return (m_pObj->*m_pFunc)(params[0]);
        }

       
private:
        FuncPointer m_pFunc;
        ObjType *m_pObj;
};

template<class ObjType, class Arg1>
class SteelFunctorHandleArg : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)(Arg1);
    SteelFunctorHandleArg(ObjType *pObj, FuncPointer p)
    :m_pFunc(p),m_pObj(pObj){}
    virtual ~SteelFunctorHandleArg(){}
    virtual SteelType Call(SteelInterpreter*,const SteelType::Container &params)
    {
	if(params.size() != 1) throw ParamMismatch();
	
	    SteelType::Handle handle = params[0];
	    Arg1 arg1 = dynamic_cast<Arg1>(handle);
	    if(arg1 == NULL) throw TypeMismatch();
	
	    return (m_pObj->*m_pFunc)(arg1);
    }
private:
    FuncPointer m_pFunc;
    ObjType *m_pObj;
};

template<class ObjType, class Arg1, class Arg2>
    class SteelFunctor2Arg : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)(Arg1,Arg2);
    SteelFunctor2Arg(ObjType *pObj, FuncPointer p)
        :m_pFunc(p),m_pObj(pObj){}
        virtual ~SteelFunctor2Arg(){}
        virtual SteelType Call(SteelInterpreter*,const SteelType::Container &params)
        {
            if(params.size() != 2) throw ParamMismatch();
            return (m_pObj->*m_pFunc)(params[0],params[1]);
        }

private:
        FuncPointer  m_pFunc;
        ObjType * m_pObj;
};

template<class ObjType, class Arg1, class Arg2, class Arg3>
    class SteelFunctor3Arg : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)(Arg1,Arg2,Arg3);
    SteelFunctor3Arg(ObjType *pObj, FuncPointer p)
        :m_pFunc(p),m_pObj(pObj){}
        virtual ~SteelFunctor3Arg(){}
        virtual SteelType Call(SteelInterpreter *,const SteelType::Container &params)
        {
            if(params.size() != 3) throw ParamMismatch();
            return (m_pObj->*m_pFunc)(params[0],
                                      params[1],
                                      params[2]);
        }
private:
        FuncPointer m_pFunc;
        ObjType *m_pObj;
};

template<class ObjType, class Arg1, class Arg2, class Arg3, class Arg4>
    class SteelFunctor4Arg : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)(Arg1,Arg2,Arg3,Arg4);
    SteelFunctor4Arg(ObjType *pObj, FuncPointer p):
        m_pFunc(p),m_pObj(pObj){}
        virtual ~SteelFunctor4Arg(){}
        virtual SteelType Call(SteelInterpreter*,const SteelType::Container &params)
        {
            if(params.size() != 4) throw ParamMismatch();
            return (m_pObj->*m_pFunc)(params[0],
                                      params[1],
                                      params[2],
                                      params[3]);
        }
private:
        FuncPointer m_pFunc;
        ObjType *m_pObj;
};


template<class ObjType, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5>
    class SteelFunctor5Arg : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)(Arg1,Arg2,Arg3,Arg4,Arg5);
    SteelFunctor5Arg(ObjType *pObj, FuncPointer p):
        m_pFunc(p),m_pObj(pObj){}
        virtual ~SteelFunctor5Arg(){}
        virtual SteelType Call(SteelInterpreter*,const SteelType::Container &params)
        {
            if(params.size() != 5) throw ParamMismatch();
            return (m_pObj->*m_pFunc)(params[0],
                                      params[1],
                                      params[2],
                                      params[3],
                                      params[4]
                                     );
        }
private:
        FuncPointer m_pFunc;
        ObjType *m_pObj;
};
template<class ObjType, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5,class Arg6>
    class SteelFunctor6Arg : public SteelFunctor
{
public:
    typedef SteelType (ObjType::*FuncPointer)(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6);
    SteelFunctor6Arg(ObjType *pObj, FuncPointer p):
        m_pFunc(p),m_pObj(pObj){}
        virtual ~SteelFunctor6Arg(){}
        virtual SteelType Call(SteelInterpreter*,const SteelType::Container &params)
        {
            if(params.size() != 6) throw ParamMismatch();
            return (m_pObj->*m_pFunc)(params[0],
                                      params[1],
                                      params[2],
                                      params[3],
                                      params[4],
									  params[5]
                                     );
        }
private:
        FuncPointer m_pFunc;
        ObjType *m_pObj;
};

template <class ObjType>
SteelFunctor* create_functor(ObjType* obj, SteelType (ObjType::*ObjFunctor) (void) ){
	return new SteelFunctorNoArgs<ObjType>(obj,ObjFunctor);
}

template <class ObjType, class A>
SteelFunctor* create_functor(ObjType* obj, SteelType (ObjType::*ObjFunctor) (A) ){
	return new SteelFunctor1Arg<ObjType,A>(obj,ObjFunctor);
}


template <class ObjType, class A, class B>
  SteelFunctor* create_functor(ObjType* obj, SteelType (ObjType::*ObjFunctor) (A,B) ){
	return new SteelFunctor2Arg<ObjType,A,B>(obj,ObjFunctor);
}

template <class ObjType, class A, class B, class C>
  SteelFunctor* create_functor(ObjType* obj, SteelType (ObjType::*ObjFunctor) (A,B,C) ){
	return new SteelFunctor3Arg<ObjType,A,B,C>(obj,ObjFunctor);
}

template <class ObjType, class A, class B, class C, class D>
  SteelFunctor* create_functor(ObjType* obj, SteelType (ObjType::*ObjFunctor) (A,B,C,D) ){
	return new SteelFunctor4Arg<ObjType,A,B,C,D>(obj,ObjFunctor);
}

template <class ObjType, class A, class B, class C, class D, class E>
  SteelFunctor* create_functor(ObjType* obj, SteelType (ObjType::*ObjFunctor) (A,B,C,D,E) ){
	return new SteelFunctor5Arg<ObjType,A,B,C,D,E>(obj,ObjFunctor);
}

template <class ObjType, class A, class B, class C, class D, class E, class F>
  SteelFunctor* create_functor(ObjType* obj, SteelType (ObjType::*ObjFunctor) (A,B,C,D,E,F) ){
	return new SteelFunctor6Arg<ObjType,A,B,C,D,E,F>(obj,ObjFunctor);
}


template<class ObjType>
class SteelFunctorArray : public SteelFunctor
{
 public:
  typedef SteelType (ObjType::*FuncPointer)(const SteelArray&);
 SteelFunctorArray(ObjType* obj,FuncPointer p):
  m_pObj(obj),m_pFunc(p){}
  virtual ~SteelFunctorArray(){}
  virtual SteelType Call(SteelInterpreter*,const SteelArray &params)
  {
    return (m_pObj->*m_pFunc)(params);
  }
 private:
  FuncPointer m_pFunc;
  ObjType* m_pObj;
};


template <class T>
T GrabHandle(SteelType::Handle handle)
{
    T t = dynamic_cast<T>(handle);
    if(t == NULL) 
		throw TypeMismatch();
    return t;
}
/*

template <class Class>
class Thunk
{
public:
typedef SteelType (Class::*MemberFunction)() ;
Thunk(const vector<SteelType> *pParams,Class *pObject, SteelFunctor f);
virtual ~Thunk();

private:
};

*/
}
#endif


