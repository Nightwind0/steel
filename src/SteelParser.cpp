#include "SteelParser.h"

#include <cassert>
#include <cstdio>
#include <iomanip>
#include <iostream>

#define DEBUG_SPEW_1(x) if (m_debug_spew_level >= 1) std::cerr << x
#define DEBUG_SPEW_2(x) if (m_debug_spew_level >= 2) std::cerr << x


#line 28 "steel.trison"

	#include "SteelScanner.h"
	#include "Ast.h"
	#include <sstream>

	std::string itos(int i)	// convert int to string
	{
		std::stringstream s;
		s << i;
		return s.str();
	}
#define GET_LINE() m_scanner->getCurrentLine()
#define GET_SCRIPT() m_scanner->getScriptName()

#line 28 "SteelParser.cpp"

SteelParser::SteelParser ()

{

#line 70 "steel.trison"

    m_scanner = new SteelScanner();

#line 38 "SteelParser.cpp"
    m_debug_spew_level = 0;
    DEBUG_SPEW_2("### number of state transitions = " << ms_state_transition_count << std::endl);
    m_reduction_token = static_cast<AstBase*>(0);
}

SteelParser::~SteelParser ()
{

#line 74 "steel.trison"

    delete m_scanner;

#line 51 "SteelParser.cpp"
}

void SteelParser::CheckStateConsistency ()
{
    unsigned int counter = 1;
    for (unsigned int i = 0; i < ms_state_count; ++i)
    {
        if (ms_state[i].m_lookahead_transition_offset > 0)
        {
            assert(counter == ms_state[i].m_lookahead_transition_offset);
            assert(ms_state[i].m_lookahead_transition_count > 0);
        }
        else
            assert(ms_state[i].m_lookahead_transition_count == 0);

        counter += ms_state[i].m_lookahead_transition_count;

        if (ms_state[i].m_default_action_offset > 0)
            ++counter;

        if (ms_state[i].m_non_terminal_transition_offset > 0)
        {
            assert(counter == ms_state[i].m_non_terminal_transition_offset);
            assert(ms_state[i].m_non_terminal_transition_offset > 0);
        }
        else
            assert(ms_state[i].m_non_terminal_transition_offset == 0);

        counter += ms_state[i].m_non_terminal_transition_count;
    }
    assert(counter == ms_state_transition_count);
}

SteelParser::ParserReturnCode SteelParser::Parse ()
{

#line 43 "steel.trison"

	mbErrorEncountered = false;
	mErrors.clear();

#line 93 "SteelParser.cpp"

    ParserReturnCode return_code = PrivateParse();



    return return_code;
}

bool SteelParser::GetDoesStateAcceptErrorToken (SteelParser::StateNumber state_number) const
{
    assert(state_number < ms_state_count);
    State const &state = ms_state[state_number];

    for (unsigned int transition = state.m_lookahead_transition_offset,
                      transition_end = state.m_lookahead_transition_offset +
                                       state.m_lookahead_transition_count;
         transition < transition_end;
         ++transition)
    {
        if (ms_state_transition[transition].m_token_type == Token::ERROR_)
            return true;
    }

    return false;
}

SteelParser::ParserReturnCode SteelParser::PrivateParse ()
{
    m_state_stack.clear();
    m_token_stack.clear();

    m_lookahead_token_type = Token::INVALID_;
    m_lookahead_token = static_cast<AstBase*>(0);
    m_is_new_lookahead_token_required = true;
    m_in_error_handling_mode = false;

    m_is_returning_with_non_terminal = false;
    m_returning_with_this_non_terminal = Token::INVALID_;

    // start in state 0
    PushState(0);

    while (true)
    {
        StateNumber current_state_number = m_state_stack.back();
        assert(current_state_number < ms_state_count);
        State const &current_state = ms_state[current_state_number];

        unsigned int state_transition_number;
        unsigned int state_transition_count;
        unsigned int default_action_state_transition_number;
        Token::Type state_transition_token_type = Token::INVALID_;

        // if we've just reduced to a non-terminal, coming from
        // another state, use the non-terminal transitions.
        if (m_is_returning_with_non_terminal)
        {
            m_is_returning_with_non_terminal = false;
            state_transition_number = current_state.m_non_terminal_transition_offset;
            state_transition_count = current_state.m_non_terminal_transition_count;
            default_action_state_transition_number = 0;
            state_transition_token_type = m_returning_with_this_non_terminal;
        }
        // otherwise use the lookahead transitions, with the default action
        else
        {
            state_transition_number = current_state.m_lookahead_transition_offset;
            state_transition_count = current_state.m_lookahead_transition_count;
            default_action_state_transition_number = current_state.m_default_action_offset;
            // GetLookaheadTokenType may cause Scan to be called, which may
            // block execution.  only scan a token if necessary.
            if (state_transition_count != 0)
            {
                state_transition_token_type = GetLookaheadTokenType();
                DEBUG_SPEW_1("*** lookahead token type: " << state_transition_token_type << std::endl);
            }
        }

        unsigned int i;
        for (i = 0;
             i < state_transition_count;
             ++i, ++state_transition_number)
        {
            StateTransition const &state_transition =
                ms_state_transition[state_transition_number];
            // if this token matches the current transition, do its action
            if (m_in_error_handling_mode && state_transition.m_token_type == Token::ERROR_ && state_transition_token_type != Token::END_ ||
                !m_in_error_handling_mode && state_transition.m_token_type == state_transition_token_type)
            {
                if (state_transition.m_token_type == Token::ERROR_)
                {
                    ThrowAwayToken(m_lookahead_token);
                    m_lookahead_token = static_cast<AstBase*>(0);
                    m_lookahead_token_type = Token::ERROR_;
                }

                PrintStateTransition(state_transition_number);
                if (ProcessAction(state_transition.m_action) == ARC_ACCEPT_AND_RETURN)
                    return PRC_SUCCESS; // the accepted token is in m_reduction_token
                else
                    break;
            }
        }

        // if no transition matched, check for a default action.
        if (i == state_transition_count)
        {
            // check for the default action
            if (default_action_state_transition_number != 0)
            {
                PrintStateTransition(default_action_state_transition_number);
                Action const &default_action =
                    ms_state_transition[default_action_state_transition_number].m_action;
                if (ProcessAction(default_action) == ARC_ACCEPT_AND_RETURN)
                    return PRC_SUCCESS; // the accepted token is in m_reduction_token
            }
            // otherwise go into error recovery mode
            else
            {
                assert(!m_is_new_lookahead_token_required);
                assert(!m_in_error_handling_mode);

                DEBUG_SPEW_1("!!! error recovery: begin" << std::endl);
                m_in_error_handling_mode = true;

                // pop the stack until we reach an error-handling state, but only
                // if the lookahead token isn't END_ (to prevent an infinite loop).
                while (!GetDoesStateAcceptErrorToken(current_state_number) || m_lookahead_token_type == Token::END_)
                {
                    DEBUG_SPEW_1("!!! error recovery: popping state " << current_state_number << std::endl);
                    assert(m_token_stack.size() + 1 == m_state_stack.size());
                    if (m_token_stack.size() > 0)
                    {
                        ThrowAwayToken(m_token_stack.back());
                        m_token_stack.pop_back();
                    }
                    m_state_stack.pop_back();

                    if (m_state_stack.size() == 0)
                    {
                        DEBUG_SPEW_1("!!! error recovery: unhandled error -- quitting" << std::endl);
                        return PRC_UNHANDLED_PARSE_ERROR;
                    }

                    current_state_number = m_state_stack.back();
                }

                DEBUG_SPEW_1("!!! error recovery: found state which accepts %error token" << std::endl);
                PrintStateStack();
            }
        }
    }

    // this should never happen because the above loop is infinite
    return PRC_UNHANDLED_PARSE_ERROR;
}

SteelParser::ActionReturnCode SteelParser::ProcessAction (SteelParser::Action const &action)
{
    if (action.m_transition_action == TA_SHIFT_AND_PUSH_STATE)
    {
        m_in_error_handling_mode = false;
        ShiftLookaheadToken();
        PushState(action.m_data);
    }
    else if (action.m_transition_action == TA_PUSH_STATE)
    {
        assert(!m_in_error_handling_mode);
        PushState(action.m_data);
    }
    else if (action.m_transition_action == TA_REDUCE_USING_RULE)
    {
        assert(!m_in_error_handling_mode);
        unsigned int reduction_rule_number = action.m_data;
        assert(reduction_rule_number < ms_reduction_rule_count);
        ReductionRule const &reduction_rule = ms_reduction_rule[reduction_rule_number];
        ReduceUsingRule(reduction_rule, false);
    }
    else if (action.m_transition_action == TA_REDUCE_AND_ACCEPT_USING_RULE)
    {
        assert(!m_in_error_handling_mode);
        unsigned int reduction_rule_number = action.m_data;
        assert(reduction_rule_number < ms_reduction_rule_count);
        ReductionRule const &reduction_rule = ms_reduction_rule[reduction_rule_number];
        ReduceUsingRule(reduction_rule, true);
        DEBUG_SPEW_1("*** accept" << std::endl);
        // everything is done, so just return.
        return ARC_ACCEPT_AND_RETURN;
    }

    return ARC_CONTINUE_PARSING;
}

void SteelParser::ShiftLookaheadToken ()
{
    assert(m_lookahead_token_type != Token::DEFAULT_);
    assert(m_lookahead_token_type != Token::INVALID_);
    DEBUG_SPEW_1("*** shifting lookahead token -- type " << m_lookahead_token_type << std::endl);
    m_token_stack.push_back(m_lookahead_token);
    m_is_new_lookahead_token_required = true;
}

void SteelParser::PushState (StateNumber const state_number)
{
    assert(state_number < ms_state_count);

    DEBUG_SPEW_1("*** going to state " << state_number << std::endl);
    m_state_stack.push_back(state_number);
    PrintStateStack();
}

void SteelParser::ReduceUsingRule (ReductionRule const &reduction_rule, bool and_accept)
{
    if (and_accept)
    {
        assert(reduction_rule.m_number_of_tokens_to_reduce_by == m_state_stack.size() - 1);
        assert(reduction_rule.m_number_of_tokens_to_reduce_by == m_token_stack.size());
    }
    else
    {
        assert(reduction_rule.m_number_of_tokens_to_reduce_by < m_state_stack.size());
        assert(reduction_rule.m_number_of_tokens_to_reduce_by <= m_token_stack.size());
    }

    DEBUG_SPEW_1("*** reducing: " << reduction_rule.m_description << std::endl);

    m_is_returning_with_non_terminal = true;
    m_returning_with_this_non_terminal = reduction_rule.m_non_terminal_to_reduce_to;
    m_reduction_rule_token_count = reduction_rule.m_number_of_tokens_to_reduce_by;

    // call the reduction rule handler if it exists
    if (reduction_rule.m_handler != NULL)
        m_reduction_token = (this->*(reduction_rule.m_handler))();
    else
        m_reduction_token = static_cast<AstBase*>(0);
    // pop the states and tokens
    PopStates(reduction_rule.m_number_of_tokens_to_reduce_by, false);

    // only push the reduced token if we aren't accepting yet
    if (!and_accept)
    {
        // push the token that resulted from the reduction
        m_token_stack.push_back(m_reduction_token);
        m_reduction_token = static_cast<AstBase*>(0);
        PrintStateStack();
    }
}

void SteelParser::PopStates (unsigned int number_of_states_to_pop, bool print_state_stack)
{
    assert(m_token_stack.size() + 1 == m_state_stack.size());
    assert(number_of_states_to_pop < m_state_stack.size());
    assert(number_of_states_to_pop <= m_token_stack.size());

    while (number_of_states_to_pop-- > 0)
    {
        m_state_stack.pop_back();
        m_token_stack.pop_back();
    }

    if (print_state_stack)
        PrintStateStack();
}

void SteelParser::PrintStateStack () const
{
    DEBUG_SPEW_2("*** state stack: ");
    for (StateStack::const_iterator it = m_state_stack.begin(),
                                 it_end = m_state_stack.end();
         it != it_end;
         ++it)
    {
        DEBUG_SPEW_2(*it << " ");
    }
    DEBUG_SPEW_2(std::endl);
}

void SteelParser::PrintStateTransition (unsigned int const state_transition_number) const
{
    assert(state_transition_number < ms_state_transition_count);
    DEBUG_SPEW_2("&&& exercising state transition " << std::setw(4) << std::right << state_transition_number << std::endl);
}

void SteelParser::ScanANewLookaheadToken ()
{
    assert(!m_is_new_lookahead_token_required);
    m_lookahead_token = static_cast<AstBase*>(0);
    m_lookahead_token_type = Scan();
    DEBUG_SPEW_1("*** scanned a new lookahead token -- type " << m_lookahead_token_type << std::endl);
}

void SteelParser::ThrowAwayToken (AstBase* token)
{
    DEBUG_SPEW_1("*** throwing away token of type " << m_lookahead_token_type << std::endl);


#line 79 "steel.trison"

    delete token;

#line 394 "SteelParser.cpp"
}

void SteelParser::ThrowAwayTokenStack ()
{
    while (!m_token_stack.empty())
    {
        ThrowAwayToken(m_token_stack.back());
        m_token_stack.pop_back();
    }
}

std::ostream &operator << (std::ostream &stream, SteelParser::Token::Type token_type)
{
    static std::string const s_token_type_string[] =
    {
        "AND",
        "ARRAY_IDENTIFIER",
        "BREAK",
        "CAT",
        "CONTINUE",
        "D",
        "DECREMENT",
        "ELSE",
        "EQ",
        "FINAL",
        "FLOAT",
        "FOR",
        "FUNCTION",
        "FUNC_IDENTIFIER",
        "GT",
        "GTE",
        "IF",
        "INCREMENT",
        "INT",
        "LT",
        "LTE",
        "NE",
        "NOT",
        "OR",
        "POP",
        "RETURN",
        "STRING",
        "VAR",
        "VAR_IDENTIFIER",
        "WHILE",
        "END_",

        "array_identifier",
        "call",
        "exp",
        "exp_statement",
        "func_definition",
        "func_identifier",
        "int_literal",
        "param_definition",
        "param_id",
        "param_list",
        "root",
        "statement",
        "statement_list",
        "var_identifier",
        "vardecl",
        "START_",

        "%error",
        "DEFAULT_",
        "INVALID_"
    };
    static unsigned int const s_token_type_string_count =
        sizeof(s_token_type_string) /
        sizeof(std::string);

    unsigned token_type_value = static_cast<unsigned int>(token_type);
    if (token_type_value < 0x20)
        stream << token_type_value;
    else if (token_type_value < 0x7F)
        stream << "'" << static_cast<char>(token_type) << "'";
    else if (token_type_value < 0x100)
        stream << token_type_value;
    else if (token_type_value < 0x100 + s_token_type_string_count)
        stream << s_token_type_string[token_type_value - 0x100];
    else
        stream << token_type_value;

    return stream;
}

// ///////////////////////////////////////////////////////////////////////////
// state machine reduction rule handlers
// ///////////////////////////////////////////////////////////////////////////

// rule 0: %start <- root END_    
AstBase* SteelParser::ReductionRuleHandler0000 ()
{
    assert(0 < m_reduction_rule_token_count);
    return m_token_stack[m_token_stack.size() - m_reduction_rule_token_count];

}

// rule 1: root <- statement_list:list    
AstBase* SteelParser::ReductionRuleHandler0001 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 134 "steel.trison"

				AstScript * pScript =   new AstScript(
							list->GetLine(),
							list->GetScript());
		        pScript->SetList(list);
				return pScript;
			
#line 508 "SteelParser.cpp"
}

// rule 2: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0002 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 147 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									false);
				
#line 530 "SteelParser.cpp"
}

// rule 3: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' %error ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0003 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 157 "steel.trison"

					AstFuncIdentifier *pId = static_cast<AstFuncIdentifier*>(id);
					addError(id->GetLine(),"parser error in parameter list for function '" + pId->getValue() + '\'');
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									pId,
									NULL,
									stmts,
									false);
				
#line 552 "SteelParser.cpp"
}

// rule 4: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' %error ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0004 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 169 "steel.trison"

					AstFuncIdentifier *pId = static_cast<AstFuncIdentifier*>(id);
					addError(id->GetLine(),"parser error in parameter list for function '" + pId->getValue() + '\'');
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									pId,
									NULL,
									stmts,
									true);
				
#line 574 "SteelParser.cpp"
}

// rule 5: func_definition <- FUNCTION %error '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0005 ()
{
    assert(3 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 181 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									params,
									stmts,
									false);
				
#line 597 "SteelParser.cpp"
}

// rule 6: func_definition <- FINAL FUNCTION %error '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0006 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 194 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									params,
									stmts,
									true);
				
#line 620 "SteelParser.cpp"
}

// rule 7: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0007 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 208 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									true);
				
#line 642 "SteelParser.cpp"
}

// rule 8: param_id <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0008 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 222 "steel.trison"
 return id; 
#line 653 "SteelParser.cpp"
}

// rule 9: param_id <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0009 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 224 "steel.trison"
 return id; 
#line 664 "SteelParser.cpp"
}

// rule 10: param_definition <-     
AstBase* SteelParser::ReductionRuleHandler0010 ()
{

#line 229 "steel.trison"

		 return new AstParamDefinitionList(GET_LINE(), GET_SCRIPT());
	
#line 675 "SteelParser.cpp"
}

// rule 11: param_definition <- vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0011 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 234 "steel.trison"

				AstParamDefinitionList * pList = new AstParamDefinitionList(decl->GetLine(),decl->GetScript());
				pList->add(static_cast<AstDeclaration*>(decl));
				return pList;		
			
#line 690 "SteelParser.cpp"
}

// rule 12: param_definition <- param_definition:list ',' vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0012 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamDefinitionList* list = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 241 "steel.trison"

				list->add(static_cast<AstDeclaration*>(decl));
				return list;
			
#line 706 "SteelParser.cpp"
}

// rule 13: param_definition <- param_definition:list %error    
AstBase* SteelParser::ReductionRuleHandler0013 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamDefinitionList* list = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 247 "steel.trison"

				addError(list->GetLine(),"expected parameter definition");
				return list;
			
#line 720 "SteelParser.cpp"
}

// rule 14: statement_list <-     
AstBase* SteelParser::ReductionRuleHandler0014 ()
{

#line 256 "steel.trison"

				AstStatementList *pList = 
					new AstStatementList(m_scanner->getCurrentLine(),
										m_scanner->getScriptName());
				return pList;
			
#line 734 "SteelParser.cpp"
}

// rule 15: statement_list <- statement_list:list statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0015 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 264 "steel.trison"

					list->add( stmt ); 
					return list;
				
#line 750 "SteelParser.cpp"
}

// rule 16: statement <- %error    
AstBase* SteelParser::ReductionRuleHandler0016 ()
{

#line 272 "steel.trison"
 
			addError(GET_LINE(),"parse error");
			return new AstStatement(GET_LINE(),GET_SCRIPT());
		
#line 762 "SteelParser.cpp"
}

// rule 17: statement <- exp_statement:exp    
AstBase* SteelParser::ReductionRuleHandler0017 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 277 "steel.trison"
 return new AstExpressionStatement(exp->GetLine(),exp->GetScript(), exp); 
#line 773 "SteelParser.cpp"
}

// rule 18: statement <- func_definition:func    
AstBase* SteelParser::ReductionRuleHandler0018 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFunctionDefinition* func = static_cast< AstFunctionDefinition* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 279 "steel.trison"
 return func; 
#line 784 "SteelParser.cpp"
}

// rule 19: statement <- '{' statement_list:list '}'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0019 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 281 "steel.trison"
 return list; 
#line 795 "SteelParser.cpp"
}

// rule 20: statement <- '{' '}'    
AstBase* SteelParser::ReductionRuleHandler0020 ()
{

#line 283 "steel.trison"

			 return new AstStatement(GET_LINE(),GET_SCRIPT());
			
#line 806 "SteelParser.cpp"
}

// rule 21: statement <- vardecl:vardecl ';'    
AstBase* SteelParser::ReductionRuleHandler0021 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* vardecl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 287 "steel.trison"
 return vardecl; 
#line 817 "SteelParser.cpp"
}

// rule 22: statement <- %error vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0022 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 290 "steel.trison"

				addError(decl->GetLine(),"unexpected tokens found before variable declaration.");
				return decl;
			
#line 831 "SteelParser.cpp"
}

// rule 23: statement <- vardecl:decl %error ';'    
AstBase* SteelParser::ReductionRuleHandler0023 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 296 "steel.trison"

			addError(decl->GetLine(),"expected ';' after variable declaration.");
			return decl;
		
#line 845 "SteelParser.cpp"
}

// rule 24: statement <- WHILE '(' exp:exp ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0024 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 301 "steel.trison"
 return new AstWhileStatement(exp->GetLine(), exp->GetScript(),exp,stmt); 
#line 858 "SteelParser.cpp"
}

// rule 25: statement <- WHILE '('    
AstBase* SteelParser::ReductionRuleHandler0025 ()
{

#line 304 "steel.trison"
 
				addError(GET_LINE(),"expected ')'");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT())); 
			
#line 871 "SteelParser.cpp"
}

// rule 26: statement <- WHILE %error    
AstBase* SteelParser::ReductionRuleHandler0026 ()
{

#line 311 "steel.trison"
 
				addError(GET_LINE(),"missing loop condition.");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT())); 
			
#line 884 "SteelParser.cpp"
}

// rule 27: statement <- WHILE '(' %error ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0027 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 319 "steel.trison"
 
				addError(GET_LINE(),"error in loop expression.");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt);    
			
#line 898 "SteelParser.cpp"
}

// rule 28: statement <- IF '(' exp:exp ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0028 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 324 "steel.trison"
 return new AstIfStatement(exp->GetLine(), exp->GetScript(),exp,stmt,elses);
#line 913 "SteelParser.cpp"
}

// rule 29: statement <- IF '(' exp:exp ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0029 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 326 "steel.trison"
 return new AstIfStatement(exp->GetLine(),exp->GetScript(),exp,stmt); 
#line 926 "SteelParser.cpp"
}

// rule 30: statement <- IF '(' %error ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0030 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 329 "steel.trison"

			addError(GET_LINE(),"parse error in if condition."); 
			return new AstIfStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt,elses);
		
#line 942 "SteelParser.cpp"
}

// rule 31: statement <- IF '(' %error ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0031 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 335 "steel.trison"

				addError(GET_LINE(),"parse error in if condition."); 
				return new AstIfStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt);
			
#line 956 "SteelParser.cpp"
}

// rule 32: statement <- IF '(' %error    
AstBase* SteelParser::ReductionRuleHandler0032 ()
{

#line 341 "steel.trison"

			addError(GET_LINE(),"expected ')' after if condition.");
			return new AstIfStatement(GET_LINE(),GET_SCRIPT(),
				new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT()));
		
#line 969 "SteelParser.cpp"
}

// rule 33: statement <- IF %error    
AstBase* SteelParser::ReductionRuleHandler0033 ()
{

#line 348 "steel.trison"

			addError(GET_LINE(),"expected opening '(' after 'if'");
			return new AstIfStatement(GET_LINE(),GET_SCRIPT(),
				new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT()));

		
#line 983 "SteelParser.cpp"
}

// rule 34: statement <- RETURN exp:exp ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0034 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 356 "steel.trison"
 return new AstReturnStatement(exp->GetLine(),exp->GetScript(),exp);
#line 994 "SteelParser.cpp"
}

// rule 35: statement <- RETURN ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0035 ()
{

#line 359 "steel.trison"

				return new AstReturnStatement(GET_LINE(),GET_SCRIPT());
			
#line 1005 "SteelParser.cpp"
}

// rule 36: statement <- RETURN %error    
AstBase* SteelParser::ReductionRuleHandler0036 ()
{

#line 364 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'return'");
				return new AstReturnStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1017 "SteelParser.cpp"
}

// rule 37: statement <- RETURN    
AstBase* SteelParser::ReductionRuleHandler0037 ()
{

#line 370 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'return'");
				return new AstReturnStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1029 "SteelParser.cpp"
}

// rule 38: statement <- FOR '(' exp_statement:start exp_statement:condition ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0038 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(5 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);

#line 377 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition, 
						new AstExpression (start->GetLine(),start->GetScript()), stmt); 
			
#line 1047 "SteelParser.cpp"
}

// rule 39: statement <- FOR '(' exp_statement:start exp_statement:condition exp:iteration ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0039 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* iteration = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 383 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition,iteration,stmt);
			
#line 1066 "SteelParser.cpp"
}

// rule 40: statement <- FOR '(' %error    
AstBase* SteelParser::ReductionRuleHandler0040 ()
{

#line 388 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1080 "SteelParser.cpp"
}

// rule 41: statement <- FOR '(' exp_statement:start %error    
AstBase* SteelParser::ReductionRuleHandler0041 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 396 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1096 "SteelParser.cpp"
}

// rule 42: statement <- FOR '(' exp_statement:start exp_statement:condition %error    
AstBase* SteelParser::ReductionRuleHandler0042 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 404 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1114 "SteelParser.cpp"
}

// rule 43: statement <- FOR '(' exp_statement:start exp_statement:condition exp:iteration    
AstBase* SteelParser::ReductionRuleHandler0043 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* iteration = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 412 "steel.trison"

				addError(GET_LINE(),"malformed for loop. Expected ')'");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1134 "SteelParser.cpp"
}

// rule 44: statement <- FOR %error    
AstBase* SteelParser::ReductionRuleHandler0044 ()
{

#line 420 "steel.trison"

				addError(GET_LINE(),"malformed for loop. Expected opening '('");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1148 "SteelParser.cpp"
}

// rule 45: statement <- BREAK ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0045 ()
{

#line 429 "steel.trison"

				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1159 "SteelParser.cpp"
}

// rule 46: statement <- BREAK %error    
AstBase* SteelParser::ReductionRuleHandler0046 ()
{

#line 434 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'break'");
				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1171 "SteelParser.cpp"
}

// rule 47: statement <- BREAK    
AstBase* SteelParser::ReductionRuleHandler0047 ()
{

#line 440 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'break'");
				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1183 "SteelParser.cpp"
}

// rule 48: statement <- CONTINUE ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0048 ()
{

#line 448 "steel.trison"

				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1194 "SteelParser.cpp"
}

// rule 49: statement <- CONTINUE %error    
AstBase* SteelParser::ReductionRuleHandler0049 ()
{

#line 453 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'continue'");
				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1206 "SteelParser.cpp"
}

// rule 50: statement <- CONTINUE    
AstBase* SteelParser::ReductionRuleHandler0050 ()
{

#line 459 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'continue'");
				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1218 "SteelParser.cpp"
}

// rule 51: exp <- call:call    
AstBase* SteelParser::ReductionRuleHandler0051 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstCallExpression* call = static_cast< AstCallExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 470 "steel.trison"
 return call; 
#line 1229 "SteelParser.cpp"
}

// rule 52: exp <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0052 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 472 "steel.trison"
 return i;
#line 1240 "SteelParser.cpp"
}

// rule 53: exp <- FLOAT:f    
AstBase* SteelParser::ReductionRuleHandler0053 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* f = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 474 "steel.trison"
 return f; 
#line 1251 "SteelParser.cpp"
}

// rule 54: exp <- STRING:s    
AstBase* SteelParser::ReductionRuleHandler0054 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* s = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 476 "steel.trison"
 return s; 
#line 1262 "SteelParser.cpp"
}

// rule 55: exp <- var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0055 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 478 "steel.trison"
 return id; 
#line 1273 "SteelParser.cpp"
}

// rule 56: exp <- array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0056 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 480 "steel.trison"
 return id; 
#line 1284 "SteelParser.cpp"
}

// rule 57: exp <- exp:a '+' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0057 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 482 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,b); 
#line 1297 "SteelParser.cpp"
}

// rule 58: exp <- exp:a '+'     %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0058 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 484 "steel.trison"
 
				addError(a->GetLine(),"expected expression before '+'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1311 "SteelParser.cpp"
}

// rule 59: exp <- exp:a '-'     %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0059 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 489 "steel.trison"
 
				addError(a->GetLine(),"expected expression before '-'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1325 "SteelParser.cpp"
}

// rule 60: exp <- exp:a '*'     %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0060 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 494 "steel.trison"
 
				addError(a->GetLine(),"expected expression after '*'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1339 "SteelParser.cpp"
}

// rule 61: exp <- '*' exp:b     %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0061 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 499 "steel.trison"
 
				addError(b->GetLine(),"expected expression before '*'.");	
				return new AstBinOp(b->GetLine(),b->GetScript(),AstBinOp::MULT,new AstExpression(GET_LINE(),GET_SCRIPT()),b);
			  
#line 1353 "SteelParser.cpp"
}

// rule 62: exp <- exp:a '-' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0062 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 505 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,b); 
#line 1366 "SteelParser.cpp"
}

// rule 63: exp <- exp:a '*' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0063 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 507 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,b); 
#line 1379 "SteelParser.cpp"
}

// rule 64: exp <- exp:a '/' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0064 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 509 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::DIV,a,b); 
#line 1392 "SteelParser.cpp"
}

// rule 65: exp <- exp:a '%' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0065 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 511 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MOD,a,b); 
#line 1405 "SteelParser.cpp"
}

// rule 66: exp <- exp:a D exp:b    %left %prec POW
AstBase* SteelParser::ReductionRuleHandler0066 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 513 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::D,a,b); 
#line 1418 "SteelParser.cpp"
}

// rule 67: exp <- exp:lvalue '=' exp:exp    %right %prec ASSIGNMENT
AstBase* SteelParser::ReductionRuleHandler0067 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 515 "steel.trison"
 return new AstVarAssignmentExpression(lvalue->GetLine(),lvalue->GetScript(),lvalue,exp); 
#line 1431 "SteelParser.cpp"
}

// rule 68: exp <- exp:a '^' exp:b    %right %prec POW
AstBase* SteelParser::ReductionRuleHandler0068 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 517 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::POW,a,b); 
#line 1444 "SteelParser.cpp"
}

// rule 69: exp <- exp:a OR exp:b    %left %prec OR
AstBase* SteelParser::ReductionRuleHandler0069 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 519 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::OR,a,b); 
#line 1457 "SteelParser.cpp"
}

// rule 70: exp <- exp:a AND exp:b    %left %prec AND
AstBase* SteelParser::ReductionRuleHandler0070 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 521 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::AND,a,b); 
#line 1470 "SteelParser.cpp"
}

// rule 71: exp <- exp:a EQ exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0071 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 523 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::EQ,a,b); 
#line 1483 "SteelParser.cpp"
}

// rule 72: exp <- exp:a NE exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0072 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 525 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::NE,a,b); 
#line 1496 "SteelParser.cpp"
}

// rule 73: exp <- exp:a LT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0073 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 527 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LT,a,b); 
#line 1509 "SteelParser.cpp"
}

// rule 74: exp <- exp:a GT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0074 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 529 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GT,a,b); 
#line 1522 "SteelParser.cpp"
}

// rule 75: exp <- exp:a LTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0075 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 531 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LTE,a,b); 
#line 1535 "SteelParser.cpp"
}

// rule 76: exp <- exp:a GTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0076 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 533 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GTE,a,b); 
#line 1548 "SteelParser.cpp"
}

// rule 77: exp <- exp:a CAT exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0077 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 535 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::CAT,a,b); 
#line 1561 "SteelParser.cpp"
}

// rule 78: exp <- '(' exp:exp ')'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0078 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 537 "steel.trison"
 return exp; 
#line 1572 "SteelParser.cpp"
}

// rule 79: exp <- '-' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0079 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 539 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::MINUS,exp); 
#line 1583 "SteelParser.cpp"
}

// rule 80: exp <- '+' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0080 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 541 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::PLUS,exp); 
#line 1594 "SteelParser.cpp"
}

// rule 81: exp <- NOT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0081 ()
{

#line 543 "steel.trison"

						addError(GET_LINE(),"expected expression after unary minus.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::NOT,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1606 "SteelParser.cpp"
}

// rule 82: exp <- CAT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0082 ()
{

#line 551 "steel.trison"

						addError(GET_LINE(),"expected expression after ':'.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::CAT,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1618 "SteelParser.cpp"
}

// rule 83: exp <- NOT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0083 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 557 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::NOT,exp); 
#line 1629 "SteelParser.cpp"
}

// rule 84: exp <- CAT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0084 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 559 "steel.trison"
 return new AstUnaryOp(exp->GetLine(),
exp->GetScript(), AstUnaryOp::CAT,exp); 
#line 1641 "SteelParser.cpp"
}

// rule 85: exp <- exp:lvalue '[' exp:index ']'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0085 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* index = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 562 "steel.trison"
 return new AstArrayElement(lvalue->GetLine(),lvalue->GetScript(),lvalue,index); 
#line 1654 "SteelParser.cpp"
}

// rule 86: exp <- INCREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0086 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 564 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::PRE);
#line 1665 "SteelParser.cpp"
}

// rule 87: exp <- INCREMENT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0087 ()
{

#line 566 "steel.trison"

										addError(GET_LINE(),"expected lvalue after '++'");
										return new AstIncrement(GET_LINE(),GET_SCRIPT(),
												new AstExpression(GET_LINE(),GET_SCRIPT()),AstIncrement::PRE);
										
#line 1678 "SteelParser.cpp"
}

// rule 88: exp <- %error INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0088 ()
{

#line 572 "steel.trison"
 
									addError(GET_LINE(),"expected lvalue before '++'");
									return new AstIncrement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()), AstIncrement::POST);
	   							
#line 1690 "SteelParser.cpp"
}

// rule 89: exp <- exp:lvalue INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0089 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 578 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::POST);
#line 1701 "SteelParser.cpp"
}

// rule 90: exp <- DECREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0090 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 580 "steel.trison"
 return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::PRE);
#line 1712 "SteelParser.cpp"
}

// rule 91: exp <- DECREMENT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0091 ()
{

#line 582 "steel.trison"

										addError(GET_LINE(),"expected lvalue after '--'");
										return new AstDecrement(GET_LINE(),GET_SCRIPT(),
												new AstExpression(GET_LINE(),GET_SCRIPT()),AstDecrement::PRE);
										
#line 1725 "SteelParser.cpp"
}

// rule 92: exp <- exp:lvalue DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0092 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 589 "steel.trison"
 
									return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::POST);
									
#line 1738 "SteelParser.cpp"
}

// rule 93: exp <- %error DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0093 ()
{

#line 593 "steel.trison"
 
									addError(GET_LINE(),"expected lvalue before '--'");
									return new AstDecrement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()), AstDecrement::POST);
									
#line 1750 "SteelParser.cpp"
}

// rule 94: exp <- POP exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0094 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 599 "steel.trison"
 return new AstPop(lvalue->GetLine(),lvalue->GetScript(),lvalue); 
#line 1761 "SteelParser.cpp"
}

// rule 95: exp <- POP %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0095 ()
{

#line 601 "steel.trison"

						addError(GET_LINE(),"expected expression after 'pop'.");
						return new AstPop(GET_LINE(),GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()));
							  
#line 1773 "SteelParser.cpp"
}

// rule 96: exp_statement <- ';'    
AstBase* SteelParser::ReductionRuleHandler0096 ()
{

#line 610 "steel.trison"

			return new AstExpression(GET_LINE(),GET_SCRIPT()); 
		
#line 1784 "SteelParser.cpp"
}

// rule 97: exp_statement <- exp:exp ';'    
AstBase* SteelParser::ReductionRuleHandler0097 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 614 "steel.trison"
 return exp; 
#line 1795 "SteelParser.cpp"
}

// rule 98: int_literal <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0098 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 619 "steel.trison"
 return i; 
#line 1806 "SteelParser.cpp"
}

// rule 99: var_identifier <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0099 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 624 "steel.trison"
 return id; 
#line 1817 "SteelParser.cpp"
}

// rule 100: func_identifier <- FUNC_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0100 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 629 "steel.trison"
 return id; 
#line 1828 "SteelParser.cpp"
}

// rule 101: array_identifier <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0101 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 634 "steel.trison"
 return id; 
#line 1839 "SteelParser.cpp"
}

// rule 102: call <- func_identifier:id '(' ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0102 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 640 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id);
#line 1850 "SteelParser.cpp"
}

// rule 103: call <- func_identifier:id '(' param_list:params ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0103 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 642 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id,params);
#line 1863 "SteelParser.cpp"
}

// rule 104: call <- func_identifier:id '(' param_list:params    
AstBase* SteelParser::ReductionRuleHandler0104 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 645 "steel.trison"

				addError(GET_LINE(),"expected ')' before ';'.");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id,params); 
			
#line 1879 "SteelParser.cpp"
}

// rule 105: call <- func_identifier:id '('    
AstBase* SteelParser::ReductionRuleHandler0105 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 651 "steel.trison"

				addError(GET_LINE(),"expected ')' before ';'");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1893 "SteelParser.cpp"
}

// rule 106: call <- func_identifier:id %error ')'    
AstBase* SteelParser::ReductionRuleHandler0106 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 657 "steel.trison"

				addError(GET_LINE(),"missing '(' in function call");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1907 "SteelParser.cpp"
}

// rule 107: call <- func_identifier:id %error    
AstBase* SteelParser::ReductionRuleHandler0107 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 663 "steel.trison"

				addError(GET_LINE(),"function call missing parentheses.");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1921 "SteelParser.cpp"
}

// rule 108: call <- func_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0108 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 669 "steel.trison"

				addError(GET_LINE(),"invalid bareword. function call missing parentheses?");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1935 "SteelParser.cpp"
}

// rule 109: vardecl <- VAR var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0109 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 678 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id);
#line 1946 "SteelParser.cpp"
}

// rule 110: vardecl <- VAR var_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0110 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 680 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id,exp); 
#line 1959 "SteelParser.cpp"
}

// rule 111: vardecl <- VAR array_identifier:id '[' exp:i ']'    
AstBase* SteelParser::ReductionRuleHandler0111 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* i = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 682 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id,i); 
#line 1972 "SteelParser.cpp"
}

// rule 112: vardecl <- VAR array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0112 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 684 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id); 
#line 1983 "SteelParser.cpp"
}

// rule 113: vardecl <- VAR array_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0113 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 686 "steel.trison"

							AstArrayDeclaration *pDecl =  new AstArrayDeclaration(id->GetLine(),id->GetScript(),id);
							pDecl->assign(exp);
							return pDecl;
						 
#line 2000 "SteelParser.cpp"
}

// rule 114: param_list <- exp:exp    
AstBase* SteelParser::ReductionRuleHandler0114 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 695 "steel.trison"
 AstParamList * pList = new AstParamList ( exp->GetLine(), exp->GetScript() );
		  pList->add(exp);
		  return pList;
		
#line 2014 "SteelParser.cpp"
}

// rule 115: param_list <- param_list:list ',' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0115 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamList* list = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 700 "steel.trison"
 list->add(exp); return list;
#line 2027 "SteelParser.cpp"
}



// ///////////////////////////////////////////////////////////////////////////
// reduction rule lookup table
// ///////////////////////////////////////////////////////////////////////////

SteelParser::ReductionRule const SteelParser::ms_reduction_rule[] =
{
    {                 Token::START_,  2, &SteelParser::ReductionRuleHandler0000, "rule 0: %start <- root END_    "},
    {                 Token::root__,  1, &SteelParser::ReductionRuleHandler0001, "rule 1: root <- statement_list    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0002, "rule 2: func_definition <- FUNCTION FUNC_IDENTIFIER '(' param_definition ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0003, "rule 3: func_definition <- FUNCTION FUNC_IDENTIFIER '(' %error ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  9, &SteelParser::ReductionRuleHandler0004, "rule 4: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER '(' %error ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0005, "rule 5: func_definition <- FUNCTION %error '(' param_definition ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  9, &SteelParser::ReductionRuleHandler0006, "rule 6: func_definition <- FINAL FUNCTION %error '(' param_definition ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  9, &SteelParser::ReductionRuleHandler0007, "rule 7: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER '(' param_definition ')' '{' statement_list '}'    "},
    {             Token::param_id__,  1, &SteelParser::ReductionRuleHandler0008, "rule 8: param_id <- VAR_IDENTIFIER    "},
    {             Token::param_id__,  1, &SteelParser::ReductionRuleHandler0009, "rule 9: param_id <- ARRAY_IDENTIFIER    "},
    {     Token::param_definition__,  0, &SteelParser::ReductionRuleHandler0010, "rule 10: param_definition <-     "},
    {     Token::param_definition__,  1, &SteelParser::ReductionRuleHandler0011, "rule 11: param_definition <- vardecl    "},
    {     Token::param_definition__,  3, &SteelParser::ReductionRuleHandler0012, "rule 12: param_definition <- param_definition ',' vardecl    "},
    {     Token::param_definition__,  2, &SteelParser::ReductionRuleHandler0013, "rule 13: param_definition <- param_definition %error    "},
    {       Token::statement_list__,  0, &SteelParser::ReductionRuleHandler0014, "rule 14: statement_list <-     "},
    {       Token::statement_list__,  2, &SteelParser::ReductionRuleHandler0015, "rule 15: statement_list <- statement_list statement    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0016, "rule 16: statement <- %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0017, "rule 17: statement <- exp_statement    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0018, "rule 18: statement <- func_definition    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0019, "rule 19: statement <- '{' statement_list '}'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0020, "rule 20: statement <- '{' '}'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0021, "rule 21: statement <- vardecl ';'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0022, "rule 22: statement <- %error vardecl    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0023, "rule 23: statement <- vardecl %error ';'    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0024, "rule 24: statement <- WHILE '(' exp ')' statement     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0025, "rule 25: statement <- WHILE '('    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0026, "rule 26: statement <- WHILE %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0027, "rule 27: statement <- WHILE '(' %error ')' statement    "},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0028, "rule 28: statement <- IF '(' exp ')' statement ELSE statement     %prec ELSE"},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0029, "rule 29: statement <- IF '(' exp ')' statement     %prec NON_ELSE"},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0030, "rule 30: statement <- IF '(' %error ')' statement ELSE statement     %prec ELSE"},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0031, "rule 31: statement <- IF '(' %error ')' statement     %prec NON_ELSE"},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0032, "rule 32: statement <- IF '(' %error    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0033, "rule 33: statement <- IF %error    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0034, "rule 34: statement <- RETURN exp ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0035, "rule 35: statement <- RETURN ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0036, "rule 36: statement <- RETURN %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0037, "rule 37: statement <- RETURN    "},
    {            Token::statement__,  6, &SteelParser::ReductionRuleHandler0038, "rule 38: statement <- FOR '(' exp_statement exp_statement ')' statement     %prec CORRECT"},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0039, "rule 39: statement <- FOR '(' exp_statement exp_statement exp ')' statement     %prec CORRECT"},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0040, "rule 40: statement <- FOR '(' %error    "},
    {            Token::statement__,  4, &SteelParser::ReductionRuleHandler0041, "rule 41: statement <- FOR '(' exp_statement %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0042, "rule 42: statement <- FOR '(' exp_statement exp_statement %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0043, "rule 43: statement <- FOR '(' exp_statement exp_statement exp    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0044, "rule 44: statement <- FOR %error    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0045, "rule 45: statement <- BREAK ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0046, "rule 46: statement <- BREAK %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0047, "rule 47: statement <- BREAK    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0048, "rule 48: statement <- CONTINUE ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0049, "rule 49: statement <- CONTINUE %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0050, "rule 50: statement <- CONTINUE    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0051, "rule 51: exp <- call    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0052, "rule 52: exp <- INT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0053, "rule 53: exp <- FLOAT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0054, "rule 54: exp <- STRING    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0055, "rule 55: exp <- var_identifier    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0056, "rule 56: exp <- array_identifier    "},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0057, "rule 57: exp <- exp '+' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0058, "rule 58: exp <- exp '+'     %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0059, "rule 59: exp <- exp '-'     %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0060, "rule 60: exp <- exp '*'     %prec MULT_DIV_MOD"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0061, "rule 61: exp <- '*' exp     %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0062, "rule 62: exp <- exp '-' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0063, "rule 63: exp <- exp '*' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0064, "rule 64: exp <- exp '/' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0065, "rule 65: exp <- exp '%' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0066, "rule 66: exp <- exp D exp    %left %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0067, "rule 67: exp <- exp '=' exp    %right %prec ASSIGNMENT"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0068, "rule 68: exp <- exp '^' exp    %right %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0069, "rule 69: exp <- exp OR exp    %left %prec OR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0070, "rule 70: exp <- exp AND exp    %left %prec AND"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0071, "rule 71: exp <- exp EQ exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0072, "rule 72: exp <- exp NE exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0073, "rule 73: exp <- exp LT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0074, "rule 74: exp <- exp GT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0075, "rule 75: exp <- exp LTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0076, "rule 76: exp <- exp GTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0077, "rule 77: exp <- exp CAT exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0078, "rule 78: exp <- '(' exp ')'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0079, "rule 79: exp <- '-' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0080, "rule 80: exp <- '+' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0081, "rule 81: exp <- NOT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0082, "rule 82: exp <- CAT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0083, "rule 83: exp <- NOT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0084, "rule 84: exp <- CAT exp     %prec UNARY"},
    {                  Token::exp__,  4, &SteelParser::ReductionRuleHandler0085, "rule 85: exp <- exp '[' exp ']'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0086, "rule 86: exp <- INCREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0087, "rule 87: exp <- INCREMENT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0088, "rule 88: exp <- %error INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0089, "rule 89: exp <- exp INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0090, "rule 90: exp <- DECREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0091, "rule 91: exp <- DECREMENT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0092, "rule 92: exp <- exp DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0093, "rule 93: exp <- %error DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0094, "rule 94: exp <- POP exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0095, "rule 95: exp <- POP %error     %prec UNARY"},
    {        Token::exp_statement__,  1, &SteelParser::ReductionRuleHandler0096, "rule 96: exp_statement <- ';'    "},
    {        Token::exp_statement__,  2, &SteelParser::ReductionRuleHandler0097, "rule 97: exp_statement <- exp ';'    "},
    {          Token::int_literal__,  1, &SteelParser::ReductionRuleHandler0098, "rule 98: int_literal <- INT    "},
    {       Token::var_identifier__,  1, &SteelParser::ReductionRuleHandler0099, "rule 99: var_identifier <- VAR_IDENTIFIER    "},
    {      Token::func_identifier__,  1, &SteelParser::ReductionRuleHandler0100, "rule 100: func_identifier <- FUNC_IDENTIFIER    "},
    {     Token::array_identifier__,  1, &SteelParser::ReductionRuleHandler0101, "rule 101: array_identifier <- ARRAY_IDENTIFIER    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0102, "rule 102: call <- func_identifier '(' ')'     %prec CORRECT"},
    {                 Token::call__,  4, &SteelParser::ReductionRuleHandler0103, "rule 103: call <- func_identifier '(' param_list ')'     %prec CORRECT"},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0104, "rule 104: call <- func_identifier '(' param_list    "},
    {                 Token::call__,  2, &SteelParser::ReductionRuleHandler0105, "rule 105: call <- func_identifier '('    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0106, "rule 106: call <- func_identifier %error ')'    "},
    {                 Token::call__,  2, &SteelParser::ReductionRuleHandler0107, "rule 107: call <- func_identifier %error    "},
    {                 Token::call__,  1, &SteelParser::ReductionRuleHandler0108, "rule 108: call <- func_identifier    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0109, "rule 109: vardecl <- VAR var_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0110, "rule 110: vardecl <- VAR var_identifier '=' exp    "},
    {              Token::vardecl__,  5, &SteelParser::ReductionRuleHandler0111, "rule 111: vardecl <- VAR array_identifier '[' exp ']'    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0112, "rule 112: vardecl <- VAR array_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0113, "rule 113: vardecl <- VAR array_identifier '=' exp    "},
    {           Token::param_list__,  1, &SteelParser::ReductionRuleHandler0114, "rule 114: param_list <- exp    "},
    {           Token::param_list__,  3, &SteelParser::ReductionRuleHandler0115, "rule 115: param_list <- param_list ',' exp    "},

    // special error panic reduction rule
    {                 Token::ERROR_,  1,                                     NULL, "* -> error"}
};

unsigned int const SteelParser::ms_reduction_rule_count =
    sizeof(SteelParser::ms_reduction_rule) /
    sizeof(SteelParser::ReductionRule);

// ///////////////////////////////////////////////////////////////////////////
// state transition lookup table
// ///////////////////////////////////////////////////////////////////////////

SteelParser::State const SteelParser::ms_state[] =
{
    {   0,    0,    1,    2,    2}, // state    0
    {   4,    1,    0,    0,    0}, // state    1
    {   5,   28,    0,   33,    9}, // state    2
    {   0,    0,   42,    0,    0}, // state    3
    {  43,    3,   46,   47,    1}, // state    4
    {   0,    0,   48,    0,    0}, // state    5
    {  49,   16,    0,   65,    5}, // state    6
    {  70,    1,   71,   72,    1}, // state    7
    {  73,   16,    0,   89,    5}, // state    8
    {  94,   16,    0,  110,    5}, // state    9
    { 115,   16,    0,  131,    5}, // state   10
    { 136,   16,    0,  152,    5}, // state   11
    { 157,    2,    0,    0,    0}, // state   12
    { 159,   30,    0,    0,    0}, // state   13
    { 189,   30,    0,    0,    0}, // state   14
    { 219,   30,    0,  249,    5}, // state   15
    { 254,    2,    0,    0,    0}, // state   16
    { 256,    2,    0,    0,    0}, // state   17
    {   0,    0,  258,    0,    0}, // state   18
    {   0,    0,  259,    0,    0}, // state   19
    {   0,    0,  260,    0,    0}, // state   20
    { 261,    2,    0,    0,    0}, // state   21
    { 263,    2,    0,  265,    2}, // state   22
    {   0,    0,  267,    0,    0}, // state   23
    {   0,    0,  268,    0,    0}, // state   24
    {   0,    0,  269,    0,    0}, // state   25
    { 270,   16,    0,  286,    5}, // state   26
    { 291,   16,    0,  307,    5}, // state   27
    { 312,   16,    0,  328,    5}, // state   28
    { 333,   16,    0,  349,    5}, // state   29
    { 354,    1,    0,    0,    0}, // state   30
    {   0,    0,  355,    0,    0}, // state   31
    {   0,    0,  356,    0,    0}, // state   32
    { 357,   21,    0,    0,    0}, // state   33
    {   0,    0,  378,    0,    0}, // state   34
    {   0,    0,  379,    0,    0}, // state   35
    { 380,   47,    0,    0,    0}, // state   36
    {   0,    0,  427,    0,    0}, // state   37
    {   0,    0,  428,    0,    0}, // state   38
    { 429,    2,    0,    0,    0}, // state   39
    {   0,    0,  431,    0,    0}, // state   40
    {   0,    0,  432,    0,    0}, // state   41
    {   0,    0,  433,    0,    0}, // state   42
    { 434,    2,    0,    0,    0}, // state   43
    { 436,   21,    0,    0,    0}, // state   44
    {   0,    0,  457,    0,    0}, // state   45
    { 458,   28,    0,  486,    9}, // state   46
    { 495,    5,  500,    0,    0}, // state   47
    { 501,    5,  506,    0,    0}, // state   48
    { 507,    8,  515,    0,    0}, // state   49
    { 516,    2,  518,    0,    0}, // state   50
    { 519,    5,  524,    0,    0}, // state   51
    {   0,    0,  525,    0,    0}, // state   52
    { 526,   30,    0,  556,    5}, // state   53
    {   0,    0,  561,    0,    0}, // state   54
    {   0,    0,  562,    0,    0}, // state   55
    {   0,    0,  563,    0,    0}, // state   56
    {   0,    0,  564,    0,    0}, // state   57
    { 565,    2,  567,    0,    0}, // state   58
    {   0,    0,  568,    0,    0}, // state   59
    { 569,   21,    0,    0,    0}, // state   60
    {   0,    0,  590,    0,    0}, // state   61
    { 591,   16,    0,  607,    5}, // state   62
    { 612,    1,    0,    0,    0}, // state   63
    { 613,    1,    0,    0,    0}, // state   64
    {   0,    0,  614,    0,    0}, // state   65
    { 615,   17,    0,  632,    6}, // state   66
    { 638,    1,  639,    0,    0}, // state   67
    { 640,    2,  642,    0,    0}, // state   68
    { 643,    2,  645,    0,    0}, // state   69
    { 646,    5,  651,    0,    0}, // state   70
    { 652,    2,  654,    0,    0}, // state   71
    { 655,    5,  660,    0,    0}, // state   72
    { 661,    2,  663,    0,    0}, // state   73
    { 664,    5,  669,    0,    0}, // state   74
    { 670,    2,  672,    0,    0}, // state   75
    { 673,    5,  678,    0,    0}, // state   76
    { 679,    2,    0,    0,    0}, // state   77
    {   0,    0,  681,    0,    0}, // state   78
    { 682,   16,    0,  698,    5}, // state   79
    { 703,   16,    0,  719,    5}, // state   80
    { 724,   47,    0,  771,    5}, // state   81
    { 776,   47,    0,  823,    5}, // state   82
    { 828,   47,    0,  875,    5}, // state   83
    { 880,   16,    0,  896,    5}, // state   84
    { 901,   16,    0,  917,    5}, // state   85
    { 922,   16,    0,  938,    5}, // state   86
    { 943,   16,    0,  959,    5}, // state   87
    { 964,   16,    0,  980,    5}, // state   88
    { 985,   16,    0, 1001,    5}, // state   89
    {1006,   16,    0, 1022,    5}, // state   90
    {1027,   16,    0, 1043,    5}, // state   91
    {1048,   16,    0, 1064,    5}, // state   92
    {1069,   16,    0, 1085,    5}, // state   93
    {1090,   16,    0, 1106,    5}, // state   94
    {1111,   16,    0, 1127,    5}, // state   95
    {   0,    0, 1132,    0,    0}, // state   96
    {   0,    0, 1133,    0,    0}, // state   97
    {1134,   16,    0, 1150,    5}, // state   98
    {1155,    1, 1156,    0,    0}, // state   99
    {1157,   47,    0, 1204,    6}, // state  100
    {1210,    1,    0,    0,    0}, // state  101
    {   0,    0, 1211,    0,    0}, // state  102
    {   0,    0, 1212,    0,    0}, // state  103
    {   0,    0, 1213,    0,    0}, // state  104
    {1214,    3,    0,    0,    0}, // state  105
    {1217,   21,    0,    0,    0}, // state  106
    {   0,    0, 1238,    0,    0}, // state  107
    {1239,    3, 1242,    0,    0}, // state  108
    {1243,   21,    0,    0,    0}, // state  109
    {1264,    1, 1265, 1266,    2}, // state  110
    {1268,    4,    0, 1272,    2}, // state  111
    {1274,    2, 1276,    0,    0}, // state  112
    {1277,   17,    0, 1294,    6}, // state  113
    {1300,   16,    0, 1316,    5}, // state  114
    {1321,   16,    0, 1337,    5}, // state  115
    {1342,   16,    0, 1358,    5}, // state  116
    {1363,    1,    0,    0,    0}, // state  117
    {1364,    1,    0,    0,    0}, // state  118
    {1365,   20, 1385,    0,    0}, // state  119
    {1386,   21,    0,    0,    0}, // state  120
    {1407,    9, 1416,    0,    0}, // state  121
    {1417,    9, 1426,    0,    0}, // state  122
    {1427,    6, 1433,    0,    0}, // state  123
    {1434,    6, 1440,    0,    0}, // state  124
    {1441,    4, 1445,    0,    0}, // state  125
    {1446,    6, 1452,    0,    0}, // state  126
    {1453,    4, 1457,    0,    0}, // state  127
    {1458,   11, 1469,    0,    0}, // state  128
    {1470,   11, 1481,    0,    0}, // state  129
    {1482,   15, 1497,    0,    0}, // state  130
    {1498,   15, 1513,    0,    0}, // state  131
    {1514,   11, 1525,    0,    0}, // state  132
    {1526,   11, 1537,    0,    0}, // state  133
    {1538,   17, 1555,    0,    0}, // state  134
    {1556,   18, 1574,    0,    0}, // state  135
    {1575,    9, 1584,    0,    0}, // state  136
    {   0,    0, 1585,    0,    0}, // state  137
    {   0,    0, 1586,    0,    0}, // state  138
    {1587,   20, 1607,    0,    0}, // state  139
    {1608,    2, 1610,    0,    0}, // state  140
    {   0,    0, 1611,    0,    0}, // state  141
    {1612,   27,    0, 1639,    9}, // state  142
    {1648,   27,    0, 1675,    9}, // state  143
    {1684,   27,    0, 1711,    9}, // state  144
    {1720,   27,    0, 1747,    9}, // state  145
    {1756,    3,    0,    0,    0}, // state  146
    {   0,    0, 1759,    0,    0}, // state  147
    {1760,    1,    0,    0,    0}, // state  148
    {1761,    3,    0,    0,    0}, // state  149
    {1764,    2, 1766,    0,    0}, // state  150
    {1767,   17,    0, 1784,    5}, // state  151
    {1789,   20, 1809,    0,    0}, // state  152
    {1810,   20, 1830,    0,    0}, // state  153
    {1831,   21,    0,    0,    0}, // state  154
    {1852,    1, 1853, 1854,    2}, // state  155
    {1856,    4,    0, 1860,    2}, // state  156
    {   0,    0, 1862,    0,    0}, // state  157
    {   0,    0, 1863,    0,    0}, // state  158
    {1864,   16,    0, 1880,    5}, // state  159
    {   0,    0, 1885,    0,    0}, // state  160
    {   0,    0, 1886,    0,    0}, // state  161
    {1887,    1, 1888,    0,    0}, // state  162
    {1889,    1, 1890,    0,    0}, // state  163
    {   0,    0, 1891,    0,    0}, // state  164
    {1892,    1,    0,    0,    0}, // state  165
    {1893,    1,    0, 1894,    1}, // state  166
    {1895,    1,    0,    0,    0}, // state  167
    {1896,    1,    0,    0,    0}, // state  168
    {1897,    2, 1899,    0,    0}, // state  169
    {1900,   27,    0, 1927,    9}, // state  170
    {1936,   21, 1957,    0,    0}, // state  171
    {   0,    0, 1958,    0,    0}, // state  172
    {1959,    3,    0,    0,    0}, // state  173
    {1962,    1,    0,    0,    0}, // state  174
    {1963,    3,    0,    0,    0}, // state  175
    {1966,   20, 1986,    0,    0}, // state  176
    {1987,   27,    0, 2014,    9}, // state  177
    {2023,   27,    0, 2050,    9}, // state  178
    {   0,    0, 2059, 2060,    1}, // state  179
    {   0,    0, 2061,    0,    0}, // state  180
    {   0,    0, 2062, 2063,    1}, // state  181
    {   0,    0, 2064, 2065,    1}, // state  182
    {   0,    0, 2066,    0,    0}, // state  183
    {2067,   27,    0, 2094,    9}, // state  184
    {2103,    1,    0,    0,    0}, // state  185
    {2104,    1,    0,    0,    0}, // state  186
    {2105,    1,    0,    0,    0}, // state  187
    {   0,    0, 2106,    0,    0}, // state  188
    {   0,    0, 2107,    0,    0}, // state  189
    {2108,   28,    0, 2136,    9}, // state  190
    {2145,   28,    0, 2173,    9}, // state  191
    {2182,   28,    0, 2210,    9}, // state  192
    {   0,    0, 2219,    0,    0}, // state  193
    {   0,    0, 2220, 2221,    1}, // state  194
    {   0,    0, 2222, 2223,    1}, // state  195
    {   0,    0, 2224, 2225,    1}, // state  196
    {   0,    0, 2226,    0,    0}, // state  197
    {   0,    0, 2227,    0,    0}, // state  198
    {   0,    0, 2228,    0,    0}, // state  199
    {2229,   28,    0, 2257,    9}, // state  200
    {2266,   28,    0, 2294,    9}, // state  201
    {2303,   28,    0, 2331,    9}, // state  202
    {   0,    0, 2340,    0,    0}, // state  203
    {   0,    0, 2341,    0,    0}, // state  204
    {   0,    0, 2342,    0,    0}  // state  205

};

unsigned int const SteelParser::ms_state_count =
    sizeof(SteelParser::ms_state) /
    sizeof(SteelParser::State);

// ///////////////////////////////////////////////////////////////////////////
// state transition table
// ///////////////////////////////////////////////////////////////////////////

SteelParser::StateTransition const SteelParser::ms_state_transition[] =
{
    // dummy transition in the NULL transition
    {               Token::INVALID_, {                       TA_COUNT,    0}},

// ///////////////////////////////////////////////////////////////////////////
// state    0
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {                 Token::root__, {                  TA_PUSH_STATE,    1}},
    {       Token::statement_list__, {                  TA_PUSH_STATE,    2}},

// ///////////////////////////////////////////////////////////////////////////
// state    1
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {        TA_SHIFT_AND_PUSH_STATE,    3}},

// ///////////////////////////////////////////////////////////////////////////
// state    2
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,    1}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,   32}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state    3
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {TA_REDUCE_AND_ACCEPT_USING_RULE,    0}},

// ///////////////////////////////////////////////////////////////////////////
// state    4
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   16}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state    5
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state    6
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   44}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state    7
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,   46}},

// ///////////////////////////////////////////////////////////////////////////
// state    8
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   47}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state    9
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   48}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   10
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   49}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   11
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   50}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   51}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   12
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   52}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   53}},

// ///////////////////////////////////////////////////////////////////////////
// state   13
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   47}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   54}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   55}},
    {              Token::Type('('), {           TA_REDUCE_USING_RULE,   47}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   47}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   47}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,   47}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,   47}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,   47}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,   47}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   47}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   47}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   47}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   47}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   47}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   47}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   47}},
    {        Token::FUNC_IDENTIFIER, {           TA_REDUCE_USING_RULE,   47}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,   47}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,   47}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   47}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   47}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,   47}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,   47}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,   47}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,   47}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,   47}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,   47}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,   47}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   47}},

// ///////////////////////////////////////////////////////////////////////////
// state   14
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   50}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   56}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   57}},
    {              Token::Type('('), {           TA_REDUCE_USING_RULE,   50}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   50}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   50}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,   50}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,   50}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,   50}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,   50}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   50}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   50}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   50}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   50}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   50}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   50}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   50}},
    {        Token::FUNC_IDENTIFIER, {           TA_REDUCE_USING_RULE,   50}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,   50}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,   50}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   50}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   50}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,   50}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,   50}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,   50}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,   50}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,   50}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,   50}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,   50}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   50}},

// ///////////////////////////////////////////////////////////////////////////
// state   15
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   37}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   58}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   59}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   37}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   37}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   37}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   37}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   37}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   37}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   37}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   37}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   37}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   37}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   37}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   37}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   60}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   16
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   62}},

// ///////////////////////////////////////////////////////////////////////////
// state   17
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   64}},

// ///////////////////////////////////////////////////////////////////////////
// state   18
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  100}},

// ///////////////////////////////////////////////////////////////////////////
// state   19
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   99}},

// ///////////////////////////////////////////////////////////////////////////
// state   20
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  101}},

// ///////////////////////////////////////////////////////////////////////////
// state   21
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   66}},

// ///////////////////////////////////////////////////////////////////////////
// state   22
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   67}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   68}},

// ///////////////////////////////////////////////////////////////////////////
// state   23
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   52}},

// ///////////////////////////////////////////////////////////////////////////
// state   24
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   53}},

// ///////////////////////////////////////////////////////////////////////////
// state   25
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   54}},

// ///////////////////////////////////////////////////////////////////////////
// state   26
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   70}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   27
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   72}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   28
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   74}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   29
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   76}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   30
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   77}},

// ///////////////////////////////////////////////////////////////////////////
// state   31
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   18}},

// ///////////////////////////////////////////////////////////////////////////
// state   32
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   15}},

// ///////////////////////////////////////////////////////////////////////////
// state   33
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state   34
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   17}},

// ///////////////////////////////////////////////////////////////////////////
// state   35
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   55}},

// ///////////////////////////////////////////////////////////////////////////
// state   36
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,  108}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,  108}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,  108}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,  108}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,  108}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,  108}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,  108}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,  108}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,  108}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,  108}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,  108}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,  108}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,  108}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,  108}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,  108}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,  108}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,  108}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,  108}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,  108}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,  108}},
    {        Token::FUNC_IDENTIFIER, {           TA_REDUCE_USING_RULE,  108}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,  108}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,  108}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,  108}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,  108}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,  108}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,  108}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,  108}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,  108}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,  108}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,  108}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,  108}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,  108}},

// ///////////////////////////////////////////////////////////////////////////
// state   37
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   56}},

// ///////////////////////////////////////////////////////////////////////////
// state   38
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   51}},

// ///////////////////////////////////////////////////////////////////////////
// state   39
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state   40
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   88}},

// ///////////////////////////////////////////////////////////////////////////
// state   41
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   93}},

// ///////////////////////////////////////////////////////////////////////////
// state   42
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},

// ///////////////////////////////////////////////////////////////////////////
// state   43
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   44
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state   45
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   20}},

// ///////////////////////////////////////////////////////////////////////////
// state   46
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,   32}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state   47
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state   48
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   49
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   61}},

// ///////////////////////////////////////////////////////////////////////////
// state   50
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   81}},

// ///////////////////////////////////////////////////////////////////////////
// state   51
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   83}},

// ///////////////////////////////////////////////////////////////////////////
// state   52
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   26}},

// ///////////////////////////////////////////////////////////////////////////
// state   53
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   25}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   25}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   25}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   25}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   25}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   25}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   25}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   25}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   25}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   25}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   25}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   25}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   25}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   25}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  106}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   54
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   46}},

// ///////////////////////////////////////////////////////////////////////////
// state   55
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state   56
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   49}},

// ///////////////////////////////////////////////////////////////////////////
// state   57
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   48}},

// ///////////////////////////////////////////////////////////////////////////
// state   58
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   59
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   35}},

// ///////////////////////////////////////////////////////////////////////////
// state   60
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  107}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state   61
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   33}},

// ///////////////////////////////////////////////////////////////////////////
// state   62
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  108}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  109}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   63
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  110}},

// ///////////////////////////////////////////////////////////////////////////
// state   64
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  111}},

// ///////////////////////////////////////////////////////////////////////////
// state   65
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state   66
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  112}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  113}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   67
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  114}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  109}},

// ///////////////////////////////////////////////////////////////////////////
// state   68
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  115}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,  116}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  112}},

// ///////////////////////////////////////////////////////////////////////////
// state   69
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   87}},

// ///////////////////////////////////////////////////////////////////////////
// state   70
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   86}},

// ///////////////////////////////////////////////////////////////////////////
// state   71
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   91}},

// ///////////////////////////////////////////////////////////////////////////
// state   72
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   90}},

// ///////////////////////////////////////////////////////////////////////////
// state   73
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   82}},

// ///////////////////////////////////////////////////////////////////////////
// state   74
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   84}},

// ///////////////////////////////////////////////////////////////////////////
// state   75
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   95}},

// ///////////////////////////////////////////////////////////////////////////
// state   76
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   94}},

// ///////////////////////////////////////////////////////////////////////////
// state   77
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  117}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,  118}},

// ///////////////////////////////////////////////////////////////////////////
// state   78
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   97}},

// ///////////////////////////////////////////////////////////////////////////
// state   79
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  119}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   80
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  120}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   81
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   59}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   59}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   59}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,   59}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   59}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   59}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,   59}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,   59}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   59}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,   59}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,   59}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,   59}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,   59}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,   59}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,   59}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,   59}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,   59}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,   59}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,   59}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,   59}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,   59}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   59}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   59}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   59}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   59}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   59}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   59}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   59}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   59}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   59}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   59}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  121}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   82
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   58}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   58}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   58}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,   58}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   58}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   58}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,   58}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,   58}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   58}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,   58}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,   58}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,   58}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,   58}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,   58}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,   58}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,   58}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,   58}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,   58}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,   58}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,   58}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,   58}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   58}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   58}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   58}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   58}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   58}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   58}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   58}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   58}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   58}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   58}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  122}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   83
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   60}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   60}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   60}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,   60}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   60}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   60}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,   60}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,   60}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   60}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,   60}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,   60}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,   60}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,   60}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,   60}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,   60}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,   60}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,   60}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,   60}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,   60}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,   60}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,   60}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   60}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   60}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   60}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   60}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   60}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   60}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   60}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   60}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   60}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   60}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  123}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   84
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  124}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   85
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  125}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   86
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  126}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   87
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  127}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   88
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  128}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   89
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  129}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   90
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  130}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   91
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  131}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   92
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  132}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   93
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  133}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   94
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  134}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   95
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  135}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   96
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   89}},

// ///////////////////////////////////////////////////////////////////////////
// state   97
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   92}},

// ///////////////////////////////////////////////////////////////////////////
// state   98
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  136}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   99
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  137}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  107}},

// ///////////////////////////////////////////////////////////////////////////
// state  100
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,  105}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,  105}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  138}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,  105}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,  105}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,  105}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,  105}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,  105}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,  105}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,  105}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,  105}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,  105}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,  105}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,  105}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,  105}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,  105}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,  105}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,  105}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,  105}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,  105}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,  105}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,  105}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,  105}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,  105}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,  105}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,  105}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,  105}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,  105}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,  105}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,  105}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,  105}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  139}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {           Token::param_list__, {                  TA_PUSH_STATE,  140}},

// ///////////////////////////////////////////////////////////////////////////
// state  101
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  141}},

// ///////////////////////////////////////////////////////////////////////////
// state  102
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   21}},

// ///////////////////////////////////////////////////////////////////////////
// state  103
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   78}},

// ///////////////////////////////////////////////////////////////////////////
// state  104
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   19}},

// ///////////////////////////////////////////////////////////////////////////
// state  105
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  142}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  106
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  143}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state  107
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   34}},

// ///////////////////////////////////////////////////////////////////////////
// state  108
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  144}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   32}},

// ///////////////////////////////////////////////////////////////////////////
// state  109
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  145}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state  110
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  146}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  147}},

// ///////////////////////////////////////////////////////////////////////////
// state  111
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  148}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   10}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   10}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  149}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  147}},

// ///////////////////////////////////////////////////////////////////////////
// state  112
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  113
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  150}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  151}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  114
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  152}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  115
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  153}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  116
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  154}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  117
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  155}},

// ///////////////////////////////////////////////////////////////////////////
// state  118
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  156}},

// ///////////////////////////////////////////////////////////////////////////
// state  119
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   67}},

// ///////////////////////////////////////////////////////////////////////////
// state  120
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  157}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state  121
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   62}},

// ///////////////////////////////////////////////////////////////////////////
// state  122
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   57}},

// ///////////////////////////////////////////////////////////////////////////
// state  123
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   63}},

// ///////////////////////////////////////////////////////////////////////////
// state  124
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   64}},

// ///////////////////////////////////////////////////////////////////////////
// state  125
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   68}},

// ///////////////////////////////////////////////////////////////////////////
// state  126
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   65}},

// ///////////////////////////////////////////////////////////////////////////
// state  127
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   66}},

// ///////////////////////////////////////////////////////////////////////////
// state  128
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   74}},

// ///////////////////////////////////////////////////////////////////////////
// state  129
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   73}},

// ///////////////////////////////////////////////////////////////////////////
// state  130
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   71}},

// ///////////////////////////////////////////////////////////////////////////
// state  131
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   72}},

// ///////////////////////////////////////////////////////////////////////////
// state  132
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   76}},

// ///////////////////////////////////////////////////////////////////////////
// state  133
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   75}},

// ///////////////////////////////////////////////////////////////////////////
// state  134
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   70}},

// ///////////////////////////////////////////////////////////////////////////
// state  135
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   69}},

// ///////////////////////////////////////////////////////////////////////////
// state  136
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   77}},

// ///////////////////////////////////////////////////////////////////////////
// state  137
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state  138
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state  139
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  114}},

// ///////////////////////////////////////////////////////////////////////////
// state  140
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  158}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  159}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  104}},

// ///////////////////////////////////////////////////////////////////////////
// state  141
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   23}},

// ///////////////////////////////////////////////////////////////////////////
// state  142
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,  160}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  143
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,  161}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  144
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,  162}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  145
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,  163}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  146
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  164}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  165}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  166}},

// ///////////////////////////////////////////////////////////////////////////
// state  147
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},

// ///////////////////////////////////////////////////////////////////////////
// state  148
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  167}},

// ///////////////////////////////////////////////////////////////////////////
// state  149
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  164}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  168}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  166}},

// ///////////////////////////////////////////////////////////////////////////
// state  150
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  151
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  169}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  170}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  171}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  152
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  110}},

// ///////////////////////////////////////////////////////////////////////////
// state  153
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  113}},

// ///////////////////////////////////////////////////////////////////////////
// state  154
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  172}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state  155
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  173}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  147}},

// ///////////////////////////////////////////////////////////////////////////
// state  156
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  174}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   10}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   10}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  175}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  147}},

// ///////////////////////////////////////////////////////////////////////////
// state  157
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   85}},

// ///////////////////////////////////////////////////////////////////////////
// state  158
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state  159
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  176}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  160
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   27}},

// ///////////////////////////////////////////////////////////////////////////
// state  161
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   24}},

// ///////////////////////////////////////////////////////////////////////////
// state  162
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  177}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   31}},

// ///////////////////////////////////////////////////////////////////////////
// state  163
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  178}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   29}},

// ///////////////////////////////////////////////////////////////////////////
// state  164
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},

// ///////////////////////////////////////////////////////////////////////////
// state  165
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  179}},

// ///////////////////////////////////////////////////////////////////////////
// state  166
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,  180}},

// ///////////////////////////////////////////////////////////////////////////
// state  167
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  181}},

// ///////////////////////////////////////////////////////////////////////////
// state  168
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  182}},

// ///////////////////////////////////////////////////////////////////////////
// state  169
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  170
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,  183}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  171
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  184}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  172
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  111}},

// ///////////////////////////////////////////////////////////////////////////
// state  173
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  164}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  185}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  166}},

// ///////////////////////////////////////////////////////////////////////////
// state  174
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  186}},

// ///////////////////////////////////////////////////////////////////////////
// state  175
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  164}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  187}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  166}},

// ///////////////////////////////////////////////////////////////////////////
// state  176
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  115}},

// ///////////////////////////////////////////////////////////////////////////
// state  177
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,  188}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  178
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,  189}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  179
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  190}},

// ///////////////////////////////////////////////////////////////////////////
// state  180
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   12}},

// ///////////////////////////////////////////////////////////////////////////
// state  181
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  191}},

// ///////////////////////////////////////////////////////////////////////////
// state  182
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  192}},

// ///////////////////////////////////////////////////////////////////////////
// state  183
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  184
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,  193}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  185
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  194}},

// ///////////////////////////////////////////////////////////////////////////
// state  186
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  195}},

// ///////////////////////////////////////////////////////////////////////////
// state  187
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  196}},

// ///////////////////////////////////////////////////////////////////////////
// state  188
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   30}},

// ///////////////////////////////////////////////////////////////////////////
// state  189
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   28}},

// ///////////////////////////////////////////////////////////////////////////
// state  190
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  197}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,   32}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  191
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  198}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,   32}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  192
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  199}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,   32}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  193
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  194
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  200}},

// ///////////////////////////////////////////////////////////////////////////
// state  195
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  201}},

// ///////////////////////////////////////////////////////////////////////////
// state  196
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  202}},

// ///////////////////////////////////////////////////////////////////////////
// state  197
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    5}},

// ///////////////////////////////////////////////////////////////////////////
// state  198
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    3}},

// ///////////////////////////////////////////////////////////////////////////
// state  199
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    2}},

// ///////////////////////////////////////////////////////////////////////////
// state  200
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  203}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,   32}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  201
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  204}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,   32}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  202
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  205}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   31}},
    {            Token::statement__, {                  TA_PUSH_STATE,   32}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   33}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   34}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   35}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   36}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   37}},
    {                 Token::call__, {                  TA_PUSH_STATE,   38}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  203
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    6}},

// ///////////////////////////////////////////////////////////////////////////
// state  204
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    4}},

// ///////////////////////////////////////////////////////////////////////////
// state  205
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    7}}

};

unsigned int const SteelParser::ms_state_transition_count =
    sizeof(SteelParser::ms_state_transition) /
    sizeof(SteelParser::StateTransition);


#line 48 "steel.trison"


void SteelParser::addError(unsigned int line, const std::string &error)
{
	mbErrorEncountered = true;
	std::string error_text = GET_SCRIPT() + ':' + itos(line) + ": " + error + '\n';
	mErrors =  mErrors + error_text;
}

void SteelParser::setBuffer(const char * pBuffer, const std::string &name)
{	
	assert( NULL != m_scanner );
	m_scanner->setBuffer(pBuffer,name);
}


SteelParser::Token::Type SteelParser::Scan ()
{
	assert(m_scanner != NULL);
	return m_scanner->Scan(&m_lookahead_token);
}

#line 5912 "SteelParser.cpp"

