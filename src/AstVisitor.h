

#ifndef _AST_VISITOR__H
#define _AST_VISITOR__H

#include "Ast.h"


namespace Steel {
	class AstVisitor {
		AstVisitor(){}
		virtual ~AstVisitor(){}
		virtual void visit(AstBase& object){
		}
		virtual void visit(AstBareword& object){
		}
		virtual void visit(
	};
}

#endif