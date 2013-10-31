#ifndef STEEL_AST_H
#define STEEL_AST_H

#include <string>
#include <list>
#include <iostream>
#include <memory>
#ifndef _WINDOWS
#include <tr1/memory>
#endif
#include "SteelType.h"


using std::ostream;
using std::tr1::shared_ptr;

namespace Steel { 
	class SteelInterpreter;
	class SteelFunctor;
	
	class AstVisitor;
	class AstIdentifier;
	
	class AstBase
	{
	public:
		AstBase(unsigned int line, const std::string &script);
		virtual ~AstBase();
		
		unsigned int GetLine() const { return  m_line; }
		const std::string GetScript() const { return m_script_name; }
		virtual ostream & print(ostream &out){ return out;}
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);
	private:
		unsigned int m_line;
		std::string m_script_name;
		friend ostream & operator<<(ostream &,AstBase&);
	};
	
	class AstBareword : public AstBase
	{
	public:
		AstBareword(unsigned int line, const std::string &script, const std::string &word)
		:AstBase(line,script),m_word(word)
		{
		}
		virtual ~AstBareword(){}
		
		std::string GetWord(void)const
		{
			return m_word;
		}
	private:
		std::string m_word;
	};
	
	class AstKeyword : public AstBase
	{
	public:
		AstKeyword(unsigned int line, const std::string &script):AstBase(line,script){}
		virtual ~AstKeyword(){}
		
		virtual ostream & print(ostream &out){ return out;}
	private:
	};
	
	class AstExpression;
	
	class AstStatement : public AstBase
	{
	public:
		AstStatement(unsigned int line, const std::string &script);
		virtual ~AstStatement();
		
		enum eStopType
		{
			BREAK,
			RETURN,
			CONTINUE,
			COMPLETED
		};
		
		virtual ostream & print(ostream &out);
		virtual eStopType execute(SteelInterpreter *pInterpreter){ return COMPLETED;}  
	private:
	};
	
	
	class AstStatementList;
	class AstFunctionDefinitionList;
	
	class AstScript : public AstStatement
	{
	public:
		AstScript(unsigned int line, const std::string &script);
		virtual ~AstScript();
		
		virtual ostream & print(std::ostream &out);
		void SetList( AstStatementList * pStatement);
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);
	private:
		
		AstStatementList *m_pList;
	};
	
	class AstExpressionStatement : public AstStatement
	{
	public:
		AstExpressionStatement(unsigned int line, const std::string& script, AstExpression* pExp);
		virtual ~AstExpressionStatement();
		
		virtual ostream & print(std::ostream &out);
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  
		void setTopLevel();
	private:
		AstExpression* m_pExp;
	};
	
	
	class AstStatementList : public AstStatement
	{
	public:
		AstStatementList(unsigned int line, const std::string &script);
		virtual ~AstStatementList();
		
		virtual ostream & print(std::ostream &out);
		void add(AstStatement *pStatement) { m_list.push_back(pStatement); }
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
		void setTopLevel();
	private:
		std::list<AstStatement*> m_list;
		bool m_bTopLevel;
	};
	
	class AstWhileStatement : public AstStatement
	{
	public:
		AstWhileStatement(unsigned int line, const std::string &script, AstExpression *pExp, AstStatement *pStmt);
		virtual ~AstWhileStatement();
		
		virtual ostream & print(std::ostream &out);
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		AstExpression * m_pCondition;
		AstStatement * m_pStatement;
	};
	
	class AstDoStatement : public AstStatement
	{
	public:
		AstDoStatement(unsigned int line, const std::string &script, AstExpression *pExp, AstStatement *pStmt);
		virtual ~AstDoStatement();
		
		virtual ostream & print(std::ostream &out);
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		AstExpression * m_pCondition;
		AstStatement * m_pStatement;
	};
	
	
	class AstIfStatement : public AstStatement
	{
	public:
		AstIfStatement(unsigned int line, const std::string &script,
					   AstExpression *pExp, AstStatement *pStmt,
				 AstStatement *pElse=NULL);
		virtual ~AstIfStatement();
		
		virtual ostream & print(std::ostream &out);
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		AstExpression *m_pCondition;
		AstStatement *m_pElse;
		AstStatement * m_pStatement;
	};
	
	
	class AstCaseStatement : public AstStatement
	{
	public:
		AstCaseStatement(unsigned int line, const std::string &script, AstStatement* statement);
		virtual ~AstCaseStatement();
		
		virtual ostream& print(std::ostream& out);
		virtual eStopType execute(SteelInterpreter* pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		AstStatement* m_pStatement;
	};
	
	class AstCaseStatementList: public AstBase
	{
	public:
		AstCaseStatementList(unsigned int line, const std::string& script);
		virtual ~AstCaseStatementList();
		
		virtual ostream& print(std::ostream& out);
		void add(AstExpression* matchExpression, AstCaseStatement* statement);
		bool setDefault(AstCaseStatement* statement);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
		AstStatement::eStopType executeCaseMatching(AstExpression* value, SteelInterpreter* pInterpreter);
	private:
		struct Case{
			AstExpression* matchExpression;
			AstCaseStatement* statement;
		};
		std::list<Case> m_cases;
		AstCaseStatement* m_pDefault;
		
	};
	
	class AstSwitchStatement: public AstStatement
	{
	public:
		AstSwitchStatement(unsigned int line, const std::string& script,
						   AstExpression* value, AstCaseStatementList* cases);
		
		virtual ostream& print(std::ostream& out);
		virtual eStopType execute(SteelInterpreter* pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		AstExpression* m_pValue;
		AstCaseStatementList* m_pCases;
	};
	
	
	class AstReturnStatement : public AstStatement
	{
	public:
		AstReturnStatement(unsigned int line, const std::string &script, AstExpression *pExp = NULL);
		virtual ~AstReturnStatement();
		
		virtual ostream & print(std::ostream &out);
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		AstExpression *m_pExpression;
	};
	
	
	class AstBreakStatement : public AstStatement
	{
	public:
		AstBreakStatement(unsigned int line, const std::string &script)
		:AstStatement(line,script){}
		virtual ~AstBreakStatement(){}
		
		virtual ostream & print(std::ostream &out){ out << "break;" << std::endl ; return out;}
		virtual eStopType execute(SteelInterpreter *pInterpreter) { return BREAK; }
	private:
		
	};
	
	class AstContinueStatement : public AstStatement
	{
	public:
		AstContinueStatement(unsigned int line, const std::string &script)
		:AstStatement(line,script){}
		virtual ~AstContinueStatement(){}
		
		virtual ostream & print(std::ostream &out){ out << "continue;" << std::endl; return out; }
		virtual eStopType execute(SteelInterpreter *pInterpreter) { return CONTINUE; }
	private:
		
	};
	
	class AstVarIdentifier;
	
	class AstLoopStatement : public AstStatement
	{
	public:
		AstLoopStatement(unsigned int line, const std::string &script,
						 AstExpression *pStart, AstExpression *pCondition,
				   AstExpression *pIteration, AstStatement * pStmt);
		
		virtual ~AstLoopStatement();
		
		virtual ostream & print(std::ostream &out);
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		AstExpression *m_pStart;
		AstExpression *m_pCondition;
		AstExpression *m_pIteration;
		AstStatement * m_pStatement;
	};
	
	class AstDeclaration;
	class AstVarIdentifier;
	class AstForEachStatement : public AstStatement
	{
	public:
		AstForEachStatement(unsigned int line, const std::string& script,
							AstDeclaration* decl, AstExpression * array_exp, AstStatement * stmt);
		AstForEachStatement(unsigned int line, const std::string& script,
							AstVarIdentifier* lvalue, AstExpression* array_exp, AstStatement *stmt);
		virtual ~AstForEachStatement();
		
		virtual ostream& print(std::ostream &out);
		virtual eStopType execute(SteelInterpreter* pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		AstDeclaration* m_pDeclaration;
		AstVarIdentifier* m_pLValue;
		AstExpression* m_pArrayExpression;
		AstStatement* m_pStatement;
	};
	
	
	class AstExpression : public AstBase
	{
	public:
		AstExpression(unsigned int line,
					  const std::string &script)
		:AstBase(line,script){}
		virtual ~AstExpression(){}
		
		virtual SteelType evaluate(SteelInterpreter *pInterpreter){ return SteelType(); }
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter){ return NULL; }
	private:
	};
	
	
	class AstInteger : public AstExpression
	{
	public:
		AstInteger(unsigned int line, const std::string &script, int value);
		virtual ~AstInteger(){}
		
		virtual ostream & print(std::ostream &out);
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter){ return NULL; }
	private:
		int m_value;
	};
	
	class AstString : public AstExpression
	{
	public:
		AstString(unsigned int line,
				  const std::string &script);
		virtual ~AstString(){}
		static char getEscapedChar(char);
		virtual ostream & print(std::ostream &out);
		void addChar(const char c);
		void addString(const std::string &str);
		std::string getString() const { return m_value; }
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter){ return NULL; }
	private:
		
		std::string m_value;
	};
	
	class AstFloat : public AstExpression
	{
	public:
		AstFloat(unsigned int line,
				 const std::string &script,
		   double value);
		virtual ~AstFloat(){}
		
		virtual ostream & print(std::ostream &out);
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter){ return NULL; }
	private:
		double m_value;
	};
	
	class AstBoolean : public AstExpression
	{
	public:
		AstBoolean(unsigned int line,
				   const std::string &script,
			 bool value);
		virtual ~AstBoolean(){}
		
		virtual ostream & print(std::ostream &out);
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter){ return NULL; }
	private:
		bool m_bValue;
	};
	
	class AstIdentifier : public AstExpression
	{
	public:
		AstIdentifier(unsigned int line,
					  const std::string &script,
				const std::string &value);
		virtual ~AstIdentifier(){}
		virtual ostream & print(std::ostream &out);
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		std::string getValue() { return m_value; }
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter){ return NULL; }
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  				
	private:
		std::string m_value;
	};
	
	class AstVarIdentifier : public AstIdentifier
	{
	public:
		AstVarIdentifier(unsigned int line,
						 const std::string &script,
				   const std::string &value)
		:AstIdentifier(line,script,value),m_pLValue(NULL){}
		virtual ~AstVarIdentifier(){}
		
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
	private:
		SteelType *m_pLValue;
	};
	
	class AstFuncIdentifier : public AstIdentifier
	{
	public:
		AstFuncIdentifier(unsigned int line,
						  const std::string &script,
					const std::string &value)
		:AstIdentifier(line,script,value)
		{
		}
		AstFuncIdentifier(unsigned int line,
						  const std::string &script,
					const std::string &value,
					const std::string &ns)
		:AstIdentifier(line,script,value),m_ns(ns)
		{
		}
		virtual ~AstFuncIdentifier(){}
		
		virtual SteelType evaluate(SteelInterpreter* pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter){ return NULL; }
		
		std::string GetNamespace(void) const;
	private:
		std::string m_ns;
	};
	
	
	class AstArrayIdentifier : public AstIdentifier
	{
	public:
		AstArrayIdentifier(unsigned int line,
						   const std::string &script,
					 const std::string &value)
		:AstIdentifier(line,script,value){}
		virtual ~AstArrayIdentifier(){}
		
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
	private:
	};
	
	
	class AstHashMapIdentifier : public AstIdentifier
	{
	public:
		AstHashMapIdentifier(unsigned int line,
							 const std::string &script,
					   const std::string &value)
		:AstIdentifier(line,script,value){}
		virtual ~AstHashMapIdentifier(){}
		
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
	private:
	};
	
	
	class AstPair : public AstBase
	{
	public:
		AstPair(unsigned int line, const std::string& script, AstExpression* key, AstExpression* value);
		virtual ~AstPair();
		
		std::string GetKey(SteelInterpreter* pInterpreter);
		SteelType GetValue(SteelInterpreter* pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		AstExpression* m_key;
		AstExpression* m_value;
	};
	
	class AstPairList: public AstExpression
	{
	public:
		AstPairList(unsigned int line, const std::string& script);
		virtual ~AstPairList();
		
		void add(AstPair* pair);
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		std::list<AstPair*> m_list;
	};
	
	
	class AstIncDec : public AstExpression
	{
	public:
		enum Order
		{
			PRE,
			POST
		};
		
		AstIncDec(unsigned int line,
				  const std::string &script,
			AstExpression *pLValue,
			Order order);
		virtual ~AstIncDec();
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	protected:
		
		AstExpression * m_pLValue;
		Order m_order;
	};
	
	class AstIncrement : public AstIncDec
	{
	public:
		AstIncrement(unsigned int line,
					 const std::string &script,
			   AstExpression *pLValue,
			   AstIncDec::Order order);
		virtual ~AstIncrement();
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
	private:
	};
	
	
	class AstDecrement : public AstIncDec
	{
	public:
		AstDecrement(unsigned int line,
					 const std::string &script,
			   AstExpression *pLValue,
			   AstIncDec::Order order);
		virtual ~AstDecrement();
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
		
	private:
	};
	
	
	class AstImport : public AstStatement
	{
	public:
		AstImport(unsigned int line, 
				  const std::string &script,
			AstString *pStr);
		virtual ~AstImport();
		virtual ostream & print(ostream &out);
		virtual eStopType execute(SteelInterpreter *pInterpreter);
	private:
		std::string m_ns;
	};
	
	
	
	class AstBinOp : public AstExpression
	{
	public:
		enum Op
		{
			ADD,
			SUB,
			MULT,
			DIV,
			MOD,
			AND,
			OR,
			D,
			POW,
			EQ,
			NE,
			LT,
			GT,
			LTE,
			GTE,
			ADD_ASSIGN,
			SUB_ASSIGN,
			MULT_ASSIGN,
			DIV_ASSIGN,
			MOD_ASSIGN,
			ASSIGN,
			BIN_AND,
			BIN_OR
		};
		
		static std::string ToString(Op op);
		
		AstBinOp(unsigned int line,
				 const std::string &script,
		   Op op, AstExpression *right=NULL, AstExpression *left=NULL);
		virtual ~AstBinOp();
		void setLeft(AstExpression* left) { m_left = left; }
		void setRight(AstExpression* right) { m_right = right; }
		virtual ostream & print(std::ostream &out);
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
		
	private:
		Op m_op;
		AstExpression *m_right;
		AstExpression *m_left;
	};
	
	class AstUnaryOp : public AstExpression
	{
	public:
		enum Op
		{
			MINUS,
			PLUS,
			NOT,
			CAT,
			BIN_NOT
		};
		static std::string ToString(Op op);
		AstUnaryOp(unsigned int line,
				   const std::string &script, Op op,
			 AstExpression *operand);
		
		virtual ~AstUnaryOp();
		virtual ostream & print(std::ostream &out);
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  				
	private:
		Op m_op;
		AstExpression *m_operand;
	};
	
	class AstPop : public AstExpression
	{
	public:
		AstPop(unsigned int line,
			   const std::string &script,
		 AstExpression *pLValue,
		 bool pop_back=false);
		virtual ~AstPop();
		
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter) { return NULL; }
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  				
	private:
		AstExpression *m_pLValue;
		bool m_bPopBack;
	};
	
	class AstPush : public AstExpression
	{
	public:
		AstPush(unsigned int line,
				const std::string &script,
		  AstExpression *pLValue,
		  AstExpression *pExp,
		  bool push_front=false);
		virtual ~AstPush();
		
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter) { return m_pLValue->lvalue(pInterpreter); }
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
		
	private:
		AstExpression *m_pLValue;
		AstExpression* m_pExp;
		bool m_bPushFront;
	};
	
	class AstRemove : public AstExpression 
	{
	public:
		AstRemove(unsigned int line,
				  const std::string &script,
			AstExpression *pLValue,
			AstExpression *pExp);
		virtual ~AstRemove();
		
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter) { return m_pLValue->lvalue(pInterpreter); }
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
		
	private:
		AstExpression *m_pLValue;
		AstExpression* m_pExp;
	};
	
	
	class AstParamList;
	
	class AstCallExpression : public AstExpression
	{
	public:
		AstCallExpression(unsigned int line,
						  const std::string &script, AstExpression *callExp, AstParamList *pList=NULL);
		virtual ~AstCallExpression();
		
		virtual ostream & print(std::ostream &out);
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		
		// In a world with no references, you cannot have a call be an lvalue.
		// However, if, in future, we added refs, then this (and possibly others)
		// Would need implementations for lvalue.
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter){ return NULL; }
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
		
	private:
		AstExpression *m_pExp;
		AstParamList *m_pParams;
		SteelType m_functor;
	};
	
	class AstArrayLiteral : public AstExpression
	{
	public:
		AstArrayLiteral(unsigned int line,
						const std::string &script);
		virtual ~AstArrayLiteral();
		
		void add(AstExpression* pExp);
		
		virtual SteelType evaluate(SteelInterpreter* pInterpreter);
		virtual SteelType* lvalue(SteelInterpreter* pInterpreter){ return NULL; }
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
		
	private:
		std::list<AstExpression*> m_list;
	};
	
	
	class AstArrayElement : public AstExpression
	{
	public:
		AstArrayElement(unsigned int line,
						const std::string &script,
				  AstExpression *pLValue,
				  AstExpression *pExp);
		virtual ~AstArrayElement();
		
		int getArrayIndex(SteelInterpreter *p) const;
		virtual ostream & print(std::ostream &out);
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
		
	private:
		AstExpression * m_pLValue;
		AstExpression * m_pExp;
	};
	
	class AstVarAssignmentExpression : public AstExpression
	{
	public:
		AstVarAssignmentExpression(unsigned int line,
								   const std::string &script,
							 AstExpression *pLValue,
							 AstExpression *pExp);
		
		virtual ~AstVarAssignmentExpression();
		
		virtual ostream & print(std::ostream &out);
		virtual SteelType evaluate(SteelInterpreter *pInterpreter);
		virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
		
	private:
		AstExpression * m_pLValue;
		AstExpression * m_pExpression;
	};
	
	
	/*
	 * class AstArrayAssignmentExpression : public AstExpression
	 * {
	 * public:
	 *	AstArrayAssignmentExpression(unsigned int line,
	 *				const std::string &script,
	 *				AstArrayIdentifier *pId,
	 *				AstExpression *pExp);
	 * 
	 *	virtual ~AstArrayAssignmentExpression();
	 * 
	 *	virtual ostream & print(std::ostream &out);
	 *	virtual SteelType evaluate(SteelInterpreter *pInterpreter);
	 *	virtual SteelType * lvalue(SteelInterpreter *pInterpreter);
	 * private:
	 *	AstArrayIdentifier * m_pId;
	 *	AstExpression * m_pExpression;
	 * 
	 };
	 
	 class AstArrayElementAssignmentExpression: public AstExpression
	 {
	 public:
		 AstArrayElementAssignmentExpression(unsigned int line,
		 const std::string &script,
		 AstArrayElement *pId,
		 AstExpression * pExp);
		 virtual ~AstArrayElementAssignmentExpression();
		 
		 virtual ostream & print (std::ostream &out);
		 virtual SteelType evaluate(SteelInterpreter *pInterpreter);
	 private:
		 AstArrayElement * m_pId;
		 AstExpression *m_pExp;
	 };
	 
	 */
	
	class AstParamList : public AstBase
	{
	public:
		AstParamList(unsigned int line,
					 const std::string &script);
		virtual ~AstParamList();
		
		void add(AstExpression *pExp);
		virtual ostream & print(std::ostream &out);
		SteelType::Container getParamList(SteelInterpreter *p) const;
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
		
	private:
		std::list<AstExpression*> m_params;
	};
	
	class AstDeclaration : public AstStatement
	{
	public:
		AstDeclaration(unsigned int line,
					   const std::string &script)
		:AstStatement(line,script),m_bHasValue(false){}
		virtual ~AstDeclaration(){}
		
		virtual void setValue(const SteelType &value);
		virtual bool hasInitializer() const { return false; }    
		virtual AstIdentifier* getIdentifier() const =0;
	protected:
		bool m_bHasValue;
		SteelType m_value;
	};
	
	class AstVarDeclaration : public AstDeclaration
	{
	public:
		AstVarDeclaration(unsigned int line,
						  const std::string &script,
					AstVarIdentifier *pId,
					AstExpression *pExp = NULL);
		AstVarDeclaration(unsigned int line,
						  const std::string &script,
					AstVarIdentifier *pId,
					bool bConst,
					AstExpression *pExp);
		
		virtual ~AstVarDeclaration();
		virtual ostream & print(std::ostream &out);
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual bool hasInitializer() const { return m_pExp != NULL; }
		virtual bool isConst() const { return m_bConst; }
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);
		virtual AstIdentifier * getIdentifier() const { return m_pId; }// not normally usd
		private:
			AstVarIdentifier *m_pId ;
			AstExpression * m_pExp;
			bool m_bConst;
		};
		
	class AstArrayDeclaration: public AstDeclaration
	{
	public:
		AstArrayDeclaration(unsigned int line,
							const std::string &script,
					AstArrayIdentifier *pId,
					AstExpression *pInt = NULL);
		
		void assign(AstExpression *pExp);
		
		virtual ~AstArrayDeclaration();
		virtual ostream & print(std::ostream &out);
		virtual bool hasInitializer() const { return m_pExp != NULL; }
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual AstIdentifier* getIdentifier() const { return m_pId; }
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
	private:
		AstArrayIdentifier *m_pId;
		AstExpression *m_pIndex;
		AstExpression *m_pExp;
	};
	
	
	class AstHashMapDeclaration: public AstDeclaration
	{
	public:
		AstHashMapDeclaration(unsigned int line,
								const std::string &script,
						AstHashMapIdentifier *pId,
						AstExpression* pExp=NULL
		);
		
		void assign(AstExpression *pExp);
		
		virtual ~AstHashMapDeclaration();
		virtual ostream & print(std::ostream &out);
		virtual bool hasInitializer() const { return m_pExp != NULL; }
		virtual eStopType execute(SteelInterpreter *pInterpreter);
		virtual AstIdentifier* getIdentifier() const { return m_pId; }
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  					
	private:
		AstHashMapIdentifier *m_pId;
		AstExpression *m_pExp;
	};
	
	
	/* A lambda definition evaluates to a pointer to a function, that 
		* function is anonymous and is defined in line
		*
		*/
	class AstParamDefinitionList;
	class AstAnonymousFunctionDefinition : public AstExpression 
	{
	public:
		AstAnonymousFunctionDefinition(unsigned int line, const std::string &script, AstParamDefinitionList* params, AstStatementList * statements);
		virtual ~AstAnonymousFunctionDefinition();
		virtual SteelType evaluate(SteelInterpreter* pInterpreter);
	private:
		shared_ptr<AstParamDefinitionList> m_pParamList;
		shared_ptr<AstStatementList> m_pStatements;
	};
	
	
	class AstParamDefinitionList : public AstBase
	{
	public:
		AstParamDefinitionList(unsigned int line,
								const std::string &script);
		virtual ~AstParamDefinitionList();
		
		void add(AstDeclaration *pId);
		virtual ostream & print (std::ostream &out);
		
		int size() const;
		// How many parameters have default values 
		int defaultCount() const;
		
		void executeDeclarations(SteelInterpreter *pInterpreter);
		void executeDeclarations(SteelInterpreter *pInterpreter, 
									const SteelType::Container &params);
		bool containsName(const std::string& id)const;
	private:
		int mnDefaults;
		std::list<AstDeclaration*> m_params;
	};
	
	class AstFunctionDefinition : public AstStatement
	{
	public:
		AstFunctionDefinition(unsigned int line,
								const std::string &script,
						AstFuncIdentifier *pId,
						AstParamDefinitionList *pParams,
						AstStatementList* pStmts);
		virtual ~AstFunctionDefinition();
		
		eStopType execute(SteelInterpreter * pInterpreter);
		virtual ostream & print (std::ostream &out);
		virtual void FindIdentifiers(std::list<AstIdentifier*>& o_ids);  		
			
	private:
		AstFuncIdentifier * m_pId;
		shared_ptr<AstParamDefinitionList> m_pParams;
		shared_ptr<AstStatementList> m_pStatements;
	};
		
}
	
#endif
	
	
	
