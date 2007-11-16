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
        "CONST",
        "CONTINUE",
        "D",
        "DECREMENT",
        "DO",
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

#line 136 "steel.trison"

				AstScript * pScript =   new AstScript(
							list->GetLine(),
							list->GetScript());
		        pScript->SetList(list);
				return pScript;
			
#line 510 "SteelParser.cpp"
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

#line 149 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									false);
				
#line 532 "SteelParser.cpp"
}

// rule 3: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' %error ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0003 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 159 "steel.trison"

					AstFuncIdentifier *pId = static_cast<AstFuncIdentifier*>(id);
					addError(id->GetLine(),"parser error in parameter list for function '" + pId->getValue() + '\'');
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									pId,
									NULL,
									stmts,
									false);
				
#line 554 "SteelParser.cpp"
}

// rule 4: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' %error ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0004 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 171 "steel.trison"

					AstFuncIdentifier *pId = static_cast<AstFuncIdentifier*>(id);
					addError(id->GetLine(),"parser error in parameter list for function '" + pId->getValue() + '\'');
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									pId,
									NULL,
									stmts,
									true);
				
#line 576 "SteelParser.cpp"
}

// rule 5: func_definition <- FUNCTION %error '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0005 ()
{
    assert(3 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 183 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									params,
									stmts,
									false);
				
#line 599 "SteelParser.cpp"
}

// rule 6: func_definition <- FINAL FUNCTION %error '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0006 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 196 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									params,
									stmts,
									true);
				
#line 622 "SteelParser.cpp"
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

#line 210 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									true);
				
#line 644 "SteelParser.cpp"
}

// rule 8: param_id <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0008 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 224 "steel.trison"
 return id; 
#line 655 "SteelParser.cpp"
}

// rule 9: param_id <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0009 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 226 "steel.trison"
 return id; 
#line 666 "SteelParser.cpp"
}

// rule 10: param_definition <-     
AstBase* SteelParser::ReductionRuleHandler0010 ()
{

#line 231 "steel.trison"

		 return new AstParamDefinitionList(GET_LINE(), GET_SCRIPT());
	
#line 677 "SteelParser.cpp"
}

// rule 11: param_definition <- vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0011 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 236 "steel.trison"

				AstParamDefinitionList * pList = new AstParamDefinitionList(decl->GetLine(),decl->GetScript());
				pList->add(static_cast<AstDeclaration*>(decl));
				return pList;		
			
#line 692 "SteelParser.cpp"
}

// rule 12: param_definition <- param_definition:list ',' vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0012 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamDefinitionList* list = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 243 "steel.trison"

				list->add(static_cast<AstDeclaration*>(decl));
				return list;
			
#line 708 "SteelParser.cpp"
}

// rule 13: param_definition <- param_definition:list %error    
AstBase* SteelParser::ReductionRuleHandler0013 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamDefinitionList* list = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 249 "steel.trison"

				addError(list->GetLine(),"expected parameter definition");
				return list;
			
#line 722 "SteelParser.cpp"
}

// rule 14: statement_list <-     
AstBase* SteelParser::ReductionRuleHandler0014 ()
{

#line 258 "steel.trison"

				AstStatementList *pList = 
					new AstStatementList(m_scanner->getCurrentLine(),
										m_scanner->getScriptName());
				return pList;
			
#line 736 "SteelParser.cpp"
}

// rule 15: statement_list <- statement_list:list statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0015 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 266 "steel.trison"

					list->add( stmt ); 
					return list;
				
#line 752 "SteelParser.cpp"
}

// rule 16: statement <- %error    
AstBase* SteelParser::ReductionRuleHandler0016 ()
{

#line 274 "steel.trison"
 
			addError(GET_LINE(),"parse error");
			return new AstStatement(GET_LINE(),GET_SCRIPT());
		
#line 764 "SteelParser.cpp"
}

// rule 17: statement <- exp_statement:exp    
AstBase* SteelParser::ReductionRuleHandler0017 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 279 "steel.trison"
 return new AstExpressionStatement(exp->GetLine(),exp->GetScript(), exp); 
#line 775 "SteelParser.cpp"
}

// rule 18: statement <- func_definition:func    
AstBase* SteelParser::ReductionRuleHandler0018 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFunctionDefinition* func = static_cast< AstFunctionDefinition* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 281 "steel.trison"
 return func; 
#line 786 "SteelParser.cpp"
}

// rule 19: statement <- '{' statement_list:list '}'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0019 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 283 "steel.trison"
 return list; 
#line 797 "SteelParser.cpp"
}

// rule 20: statement <- '{' '}'    
AstBase* SteelParser::ReductionRuleHandler0020 ()
{

#line 285 "steel.trison"

			 return new AstStatement(GET_LINE(),GET_SCRIPT());
			
#line 808 "SteelParser.cpp"
}

// rule 21: statement <- vardecl:vardecl ';'    
AstBase* SteelParser::ReductionRuleHandler0021 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* vardecl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 289 "steel.trison"
 return vardecl; 
#line 819 "SteelParser.cpp"
}

// rule 22: statement <- %error vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0022 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 292 "steel.trison"

				addError(decl->GetLine(),"unexpected tokens found before variable declaration.");
				return decl;
			
#line 833 "SteelParser.cpp"
}

// rule 23: statement <- vardecl:decl %error ';'    
AstBase* SteelParser::ReductionRuleHandler0023 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 298 "steel.trison"

			addError(decl->GetLine(),"expected ';' after variable declaration.");
			return decl;
		
#line 847 "SteelParser.cpp"
}

// rule 24: statement <- WHILE '(' exp:exp ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0024 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 303 "steel.trison"
 return new AstWhileStatement(exp->GetLine(), exp->GetScript(),exp,stmt); 
#line 860 "SteelParser.cpp"
}

// rule 25: statement <- WHILE '('    
AstBase* SteelParser::ReductionRuleHandler0025 ()
{

#line 306 "steel.trison"
 
				addError(GET_LINE(),"expected ')'");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT())); 
			
#line 873 "SteelParser.cpp"
}

// rule 26: statement <- WHILE %error    
AstBase* SteelParser::ReductionRuleHandler0026 ()
{

#line 313 "steel.trison"
 
				addError(GET_LINE(),"missing loop condition.");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT())); 
			
#line 886 "SteelParser.cpp"
}

// rule 27: statement <- WHILE '(' %error ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0027 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 321 "steel.trison"
 
				addError(GET_LINE(),"error in loop expression.");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt);    
			
#line 900 "SteelParser.cpp"
}

// rule 28: statement <- DO statement:stmt WHILE '(' exp:condition ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0028 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 327 "steel.trison"

				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), condition, stmt);
	   
#line 915 "SteelParser.cpp"
}

// rule 29: statement <- DO statement:stmt WHILE '(' %error ')'    
AstBase* SteelParser::ReductionRuleHandler0029 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 332 "steel.trison"

				addError(GET_LINE(),"error in while condition.");
				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), NULL, stmt);
	   
#line 929 "SteelParser.cpp"
}

// rule 30: statement <- DO statement:stmt %error    
AstBase* SteelParser::ReductionRuleHandler0030 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 338 "steel.trison"

				addError(GET_LINE(),"error. do loop missing proper while condition.");
				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), NULL, stmt);
	   
#line 943 "SteelParser.cpp"
}

// rule 31: statement <- DO statement:stmt WHILE '(' %error    
AstBase* SteelParser::ReductionRuleHandler0031 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 345 "steel.trison"

				addError(GET_LINE(),"error, missing condition or no closing ')' found after while.");
				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), NULL, NULL);
	   
#line 957 "SteelParser.cpp"
}

// rule 32: statement <- IF '(' exp:exp ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0032 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 352 "steel.trison"
 return new AstIfStatement(exp->GetLine(), exp->GetScript(),exp,stmt,elses);
#line 972 "SteelParser.cpp"
}

// rule 33: statement <- IF '(' exp:exp ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0033 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 354 "steel.trison"
 return new AstIfStatement(exp->GetLine(),exp->GetScript(),exp,stmt); 
#line 985 "SteelParser.cpp"
}

// rule 34: statement <- IF '(' %error ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0034 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 357 "steel.trison"

			addError(GET_LINE(),"parse error in if condition."); 
			return new AstIfStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt,elses);
		
#line 1001 "SteelParser.cpp"
}

// rule 35: statement <- IF '(' %error ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0035 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 363 "steel.trison"

				addError(GET_LINE(),"parse error in if condition."); 
				return new AstIfStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt);
			
#line 1015 "SteelParser.cpp"
}

// rule 36: statement <- IF '(' %error    
AstBase* SteelParser::ReductionRuleHandler0036 ()
{

#line 369 "steel.trison"

			addError(GET_LINE(),"expected ')' after if condition.");
			return new AstIfStatement(GET_LINE(),GET_SCRIPT(),
				new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT()));
		
#line 1028 "SteelParser.cpp"
}

// rule 37: statement <- IF %error    
AstBase* SteelParser::ReductionRuleHandler0037 ()
{

#line 376 "steel.trison"

			addError(GET_LINE(),"expected opening '(' after 'if'");
			return new AstIfStatement(GET_LINE(),GET_SCRIPT(),
				new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT()));

		
#line 1042 "SteelParser.cpp"
}

// rule 38: statement <- RETURN exp:exp ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0038 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 384 "steel.trison"
 return new AstReturnStatement(exp->GetLine(),exp->GetScript(),exp);
#line 1053 "SteelParser.cpp"
}

// rule 39: statement <- RETURN ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0039 ()
{

#line 387 "steel.trison"

				return new AstReturnStatement(GET_LINE(),GET_SCRIPT());
			
#line 1064 "SteelParser.cpp"
}

// rule 40: statement <- RETURN %error    
AstBase* SteelParser::ReductionRuleHandler0040 ()
{

#line 392 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'return'");
				return new AstReturnStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1076 "SteelParser.cpp"
}

// rule 41: statement <- RETURN    
AstBase* SteelParser::ReductionRuleHandler0041 ()
{

#line 398 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'return'");
				return new AstReturnStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1088 "SteelParser.cpp"
}

// rule 42: statement <- FOR '(' exp_statement:start exp_statement:condition ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0042 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(5 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);

#line 405 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition, 
						new AstExpression (start->GetLine(),start->GetScript()), stmt); 
			
#line 1106 "SteelParser.cpp"
}

// rule 43: statement <- FOR '(' exp_statement:start exp_statement:condition exp:iteration ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0043 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* iteration = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 411 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition,iteration,stmt);
			
#line 1125 "SteelParser.cpp"
}

// rule 44: statement <- FOR '(' %error    
AstBase* SteelParser::ReductionRuleHandler0044 ()
{

#line 416 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1139 "SteelParser.cpp"
}

// rule 45: statement <- FOR '(' exp_statement:start %error    
AstBase* SteelParser::ReductionRuleHandler0045 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 424 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1155 "SteelParser.cpp"
}

// rule 46: statement <- FOR '(' exp_statement:start exp_statement:condition %error    
AstBase* SteelParser::ReductionRuleHandler0046 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 432 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1173 "SteelParser.cpp"
}

// rule 47: statement <- FOR '(' exp_statement:start exp_statement:condition exp:iteration    
AstBase* SteelParser::ReductionRuleHandler0047 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* iteration = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 440 "steel.trison"

				addError(GET_LINE(),"malformed for loop. Expected ')'");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1193 "SteelParser.cpp"
}

// rule 48: statement <- FOR %error    
AstBase* SteelParser::ReductionRuleHandler0048 ()
{

#line 448 "steel.trison"

				addError(GET_LINE(),"malformed for loop. Expected opening '('");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1207 "SteelParser.cpp"
}

// rule 49: statement <- BREAK ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0049 ()
{

#line 457 "steel.trison"

				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1218 "SteelParser.cpp"
}

// rule 50: statement <- BREAK %error    
AstBase* SteelParser::ReductionRuleHandler0050 ()
{

#line 462 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'break'");
				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1230 "SteelParser.cpp"
}

// rule 51: statement <- BREAK    
AstBase* SteelParser::ReductionRuleHandler0051 ()
{

#line 468 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'break'");
				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1242 "SteelParser.cpp"
}

// rule 52: statement <- CONTINUE ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0052 ()
{

#line 476 "steel.trison"

				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1253 "SteelParser.cpp"
}

// rule 53: statement <- CONTINUE %error    
AstBase* SteelParser::ReductionRuleHandler0053 ()
{

#line 481 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'continue'");
				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1265 "SteelParser.cpp"
}

// rule 54: statement <- CONTINUE    
AstBase* SteelParser::ReductionRuleHandler0054 ()
{

#line 487 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'continue'");
				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1277 "SteelParser.cpp"
}

// rule 55: exp <- call:call    
AstBase* SteelParser::ReductionRuleHandler0055 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstCallExpression* call = static_cast< AstCallExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 498 "steel.trison"
 return call; 
#line 1288 "SteelParser.cpp"
}

// rule 56: exp <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0056 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 500 "steel.trison"
 return i;
#line 1299 "SteelParser.cpp"
}

// rule 57: exp <- FLOAT:f    
AstBase* SteelParser::ReductionRuleHandler0057 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* f = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 502 "steel.trison"
 return f; 
#line 1310 "SteelParser.cpp"
}

// rule 58: exp <- STRING:s    
AstBase* SteelParser::ReductionRuleHandler0058 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* s = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 504 "steel.trison"
 return s; 
#line 1321 "SteelParser.cpp"
}

// rule 59: exp <- var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0059 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 506 "steel.trison"
 return id; 
#line 1332 "SteelParser.cpp"
}

// rule 60: exp <- array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0060 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 508 "steel.trison"
 return id; 
#line 1343 "SteelParser.cpp"
}

// rule 61: exp <- exp:a '+' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0061 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 510 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,b); 
#line 1356 "SteelParser.cpp"
}

// rule 62: exp <- exp:a '+'     %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0062 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 512 "steel.trison"
 
				addError(a->GetLine(),"expected expression before '+'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1370 "SteelParser.cpp"
}

// rule 63: exp <- exp:a '-'     %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0063 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 517 "steel.trison"
 
				addError(a->GetLine(),"expected expression before '-'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1384 "SteelParser.cpp"
}

// rule 64: exp <- exp:a '*'     %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0064 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 522 "steel.trison"
 
				addError(a->GetLine(),"expected expression after '*'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1398 "SteelParser.cpp"
}

// rule 65: exp <- '*' exp:b     %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0065 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 527 "steel.trison"
 
				addError(b->GetLine(),"expected expression before '*'.");	
				return new AstBinOp(b->GetLine(),b->GetScript(),AstBinOp::MULT,new AstExpression(GET_LINE(),GET_SCRIPT()),b);
			  
#line 1412 "SteelParser.cpp"
}

// rule 66: exp <- exp:a '-' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0066 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 533 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,b); 
#line 1425 "SteelParser.cpp"
}

// rule 67: exp <- exp:a '*' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0067 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 535 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,b); 
#line 1438 "SteelParser.cpp"
}

// rule 68: exp <- exp:a '/' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0068 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 537 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::DIV,a,b); 
#line 1451 "SteelParser.cpp"
}

// rule 69: exp <- exp:a '%' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0069 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 539 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MOD,a,b); 
#line 1464 "SteelParser.cpp"
}

// rule 70: exp <- exp:a D exp:b    %left %prec POW
AstBase* SteelParser::ReductionRuleHandler0070 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 541 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::D,a,b); 
#line 1477 "SteelParser.cpp"
}

// rule 71: exp <- exp:lvalue '=' exp:exp    %right %prec ASSIGNMENT
AstBase* SteelParser::ReductionRuleHandler0071 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 543 "steel.trison"
 return new AstVarAssignmentExpression(lvalue->GetLine(),lvalue->GetScript(),lvalue,exp); 
#line 1490 "SteelParser.cpp"
}

// rule 72: exp <- exp:a '^' exp:b    %right %prec POW
AstBase* SteelParser::ReductionRuleHandler0072 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 545 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::POW,a,b); 
#line 1503 "SteelParser.cpp"
}

// rule 73: exp <- exp:a OR exp:b    %left %prec OR
AstBase* SteelParser::ReductionRuleHandler0073 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 547 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::OR,a,b); 
#line 1516 "SteelParser.cpp"
}

// rule 74: exp <- exp:a AND exp:b    %left %prec AND
AstBase* SteelParser::ReductionRuleHandler0074 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 549 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::AND,a,b); 
#line 1529 "SteelParser.cpp"
}

// rule 75: exp <- exp:a EQ exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0075 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 551 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::EQ,a,b); 
#line 1542 "SteelParser.cpp"
}

// rule 76: exp <- exp:a NE exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0076 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 553 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::NE,a,b); 
#line 1555 "SteelParser.cpp"
}

// rule 77: exp <- exp:a LT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0077 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 555 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LT,a,b); 
#line 1568 "SteelParser.cpp"
}

// rule 78: exp <- exp:a GT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0078 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 557 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GT,a,b); 
#line 1581 "SteelParser.cpp"
}

// rule 79: exp <- exp:a LTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0079 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 559 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LTE,a,b); 
#line 1594 "SteelParser.cpp"
}

// rule 80: exp <- exp:a GTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0080 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 561 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GTE,a,b); 
#line 1607 "SteelParser.cpp"
}

// rule 81: exp <- exp:a CAT exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0081 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 563 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::CAT,a,b); 
#line 1620 "SteelParser.cpp"
}

// rule 82: exp <- '(' exp:exp ')'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0082 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 565 "steel.trison"
 return exp; 
#line 1631 "SteelParser.cpp"
}

// rule 83: exp <- '-' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0083 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 567 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::MINUS,exp); 
#line 1642 "SteelParser.cpp"
}

// rule 84: exp <- '+' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0084 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 569 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::PLUS,exp); 
#line 1653 "SteelParser.cpp"
}

// rule 85: exp <- NOT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0085 ()
{

#line 571 "steel.trison"

						addError(GET_LINE(),"expected expression after unary minus.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::NOT,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1665 "SteelParser.cpp"
}

// rule 86: exp <- CAT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0086 ()
{

#line 579 "steel.trison"

						addError(GET_LINE(),"expected expression after ':'.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::CAT,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1677 "SteelParser.cpp"
}

// rule 87: exp <- NOT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0087 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 585 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::NOT,exp); 
#line 1688 "SteelParser.cpp"
}

// rule 88: exp <- CAT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0088 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 587 "steel.trison"
 return new AstUnaryOp(exp->GetLine(),
exp->GetScript(), AstUnaryOp::CAT,exp); 
#line 1700 "SteelParser.cpp"
}

// rule 89: exp <- exp:lvalue '[' exp:index ']'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0089 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* index = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 590 "steel.trison"
 return new AstArrayElement(lvalue->GetLine(),lvalue->GetScript(),lvalue,index); 
#line 1713 "SteelParser.cpp"
}

// rule 90: exp <- INCREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0090 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 592 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::PRE);
#line 1724 "SteelParser.cpp"
}

// rule 91: exp <- INCREMENT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0091 ()
{

#line 594 "steel.trison"

										addError(GET_LINE(),"expected lvalue after '++'");
										return new AstIncrement(GET_LINE(),GET_SCRIPT(),
												new AstExpression(GET_LINE(),GET_SCRIPT()),AstIncrement::PRE);
										
#line 1737 "SteelParser.cpp"
}

// rule 92: exp <- %error INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0092 ()
{

#line 600 "steel.trison"
 
									addError(GET_LINE(),"expected lvalue before '++'");
									return new AstIncrement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()), AstIncrement::POST);
	   							
#line 1749 "SteelParser.cpp"
}

// rule 93: exp <- exp:lvalue INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0093 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 606 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::POST);
#line 1760 "SteelParser.cpp"
}

// rule 94: exp <- DECREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0094 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 608 "steel.trison"
 return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::PRE);
#line 1771 "SteelParser.cpp"
}

// rule 95: exp <- DECREMENT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0095 ()
{

#line 610 "steel.trison"

										addError(GET_LINE(),"expected lvalue after '--'");
										return new AstDecrement(GET_LINE(),GET_SCRIPT(),
												new AstExpression(GET_LINE(),GET_SCRIPT()),AstDecrement::PRE);
										
#line 1784 "SteelParser.cpp"
}

// rule 96: exp <- exp:lvalue DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0096 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 617 "steel.trison"
 
									return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::POST);
									
#line 1797 "SteelParser.cpp"
}

// rule 97: exp <- %error DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0097 ()
{

#line 621 "steel.trison"
 
									addError(GET_LINE(),"expected lvalue before '--'");
									return new AstDecrement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()), AstDecrement::POST);
									
#line 1809 "SteelParser.cpp"
}

// rule 98: exp <- POP exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0098 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 627 "steel.trison"
 return new AstPop(lvalue->GetLine(),lvalue->GetScript(),lvalue); 
#line 1820 "SteelParser.cpp"
}

// rule 99: exp <- POP %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0099 ()
{

#line 629 "steel.trison"

						addError(GET_LINE(),"expected expression after 'pop'.");
						return new AstPop(GET_LINE(),GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()));
							  
#line 1832 "SteelParser.cpp"
}

// rule 100: exp_statement <- ';'    
AstBase* SteelParser::ReductionRuleHandler0100 ()
{

#line 638 "steel.trison"

			return new AstExpression(GET_LINE(),GET_SCRIPT()); 
		
#line 1843 "SteelParser.cpp"
}

// rule 101: exp_statement <- exp:exp ';'    
AstBase* SteelParser::ReductionRuleHandler0101 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 642 "steel.trison"
 return exp; 
#line 1854 "SteelParser.cpp"
}

// rule 102: int_literal <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0102 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 647 "steel.trison"
 return i; 
#line 1865 "SteelParser.cpp"
}

// rule 103: var_identifier <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0103 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 652 "steel.trison"
 return id; 
#line 1876 "SteelParser.cpp"
}

// rule 104: func_identifier <- FUNC_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0104 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 657 "steel.trison"
 return id; 
#line 1887 "SteelParser.cpp"
}

// rule 105: array_identifier <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0105 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 662 "steel.trison"
 return id; 
#line 1898 "SteelParser.cpp"
}

// rule 106: call <- func_identifier:id '(' ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0106 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 668 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id);
#line 1909 "SteelParser.cpp"
}

// rule 107: call <- func_identifier:id '(' param_list:params ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0107 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 670 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id,params);
#line 1922 "SteelParser.cpp"
}

// rule 108: call <- func_identifier:id '(' param_list:params    
AstBase* SteelParser::ReductionRuleHandler0108 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 673 "steel.trison"

				addError(GET_LINE(),"expected ')' before ';'.");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id,params); 
			
#line 1938 "SteelParser.cpp"
}

// rule 109: call <- func_identifier:id '('    
AstBase* SteelParser::ReductionRuleHandler0109 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 679 "steel.trison"

				addError(GET_LINE(),"expected ')' before ';'");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1952 "SteelParser.cpp"
}

// rule 110: call <- func_identifier:id %error ')'    
AstBase* SteelParser::ReductionRuleHandler0110 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 685 "steel.trison"

				addError(GET_LINE(),"missing '(' in function call");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1966 "SteelParser.cpp"
}

// rule 111: call <- func_identifier:id %error    
AstBase* SteelParser::ReductionRuleHandler0111 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 691 "steel.trison"

				addError(GET_LINE(),"function call missing parentheses.");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1980 "SteelParser.cpp"
}

// rule 112: call <- func_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0112 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 697 "steel.trison"

				addError(GET_LINE(),"invalid bareword. function call missing parentheses?");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1994 "SteelParser.cpp"
}

// rule 113: vardecl <- VAR var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0113 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 706 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id);
#line 2005 "SteelParser.cpp"
}

// rule 114: vardecl <- VAR var_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0114 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 708 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id,exp); 
#line 2018 "SteelParser.cpp"
}

// rule 115: vardecl <- CONST var_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0115 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 710 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id,true,exp); 
#line 2031 "SteelParser.cpp"
}

// rule 116: vardecl <- VAR array_identifier:id '[' exp:i ']'    
AstBase* SteelParser::ReductionRuleHandler0116 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* i = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 712 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id,i); 
#line 2044 "SteelParser.cpp"
}

// rule 117: vardecl <- VAR array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0117 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 714 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id); 
#line 2055 "SteelParser.cpp"
}

// rule 118: vardecl <- VAR array_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0118 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 716 "steel.trison"

							AstArrayDeclaration *pDecl =  new AstArrayDeclaration(id->GetLine(),id->GetScript(),id);
							pDecl->assign(exp);
							return pDecl;
						 
#line 2072 "SteelParser.cpp"
}

// rule 119: param_list <- exp:exp    
AstBase* SteelParser::ReductionRuleHandler0119 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 725 "steel.trison"
 AstParamList * pList = new AstParamList ( exp->GetLine(), exp->GetScript() );
		  pList->add(exp);
		  return pList;
		
#line 2086 "SteelParser.cpp"
}

// rule 120: param_list <- param_list:list ',' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0120 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamList* list = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 730 "steel.trison"
 list->add(exp); return list;
#line 2099 "SteelParser.cpp"
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
    {            Token::statement__,  6, &SteelParser::ReductionRuleHandler0028, "rule 28: statement <- DO statement WHILE '(' exp ')'     %prec CORRECT"},
    {            Token::statement__,  6, &SteelParser::ReductionRuleHandler0029, "rule 29: statement <- DO statement WHILE '(' %error ')'    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0030, "rule 30: statement <- DO statement %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0031, "rule 31: statement <- DO statement WHILE '(' %error    "},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0032, "rule 32: statement <- IF '(' exp ')' statement ELSE statement     %prec ELSE"},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0033, "rule 33: statement <- IF '(' exp ')' statement     %prec NON_ELSE"},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0034, "rule 34: statement <- IF '(' %error ')' statement ELSE statement     %prec ELSE"},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0035, "rule 35: statement <- IF '(' %error ')' statement     %prec NON_ELSE"},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0036, "rule 36: statement <- IF '(' %error    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0037, "rule 37: statement <- IF %error    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0038, "rule 38: statement <- RETURN exp ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0039, "rule 39: statement <- RETURN ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0040, "rule 40: statement <- RETURN %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0041, "rule 41: statement <- RETURN    "},
    {            Token::statement__,  6, &SteelParser::ReductionRuleHandler0042, "rule 42: statement <- FOR '(' exp_statement exp_statement ')' statement     %prec CORRECT"},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0043, "rule 43: statement <- FOR '(' exp_statement exp_statement exp ')' statement     %prec CORRECT"},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0044, "rule 44: statement <- FOR '(' %error    "},
    {            Token::statement__,  4, &SteelParser::ReductionRuleHandler0045, "rule 45: statement <- FOR '(' exp_statement %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0046, "rule 46: statement <- FOR '(' exp_statement exp_statement %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0047, "rule 47: statement <- FOR '(' exp_statement exp_statement exp    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0048, "rule 48: statement <- FOR %error    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0049, "rule 49: statement <- BREAK ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0050, "rule 50: statement <- BREAK %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0051, "rule 51: statement <- BREAK    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0052, "rule 52: statement <- CONTINUE ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0053, "rule 53: statement <- CONTINUE %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0054, "rule 54: statement <- CONTINUE    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0055, "rule 55: exp <- call    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0056, "rule 56: exp <- INT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0057, "rule 57: exp <- FLOAT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0058, "rule 58: exp <- STRING    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0059, "rule 59: exp <- var_identifier    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0060, "rule 60: exp <- array_identifier    "},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0061, "rule 61: exp <- exp '+' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0062, "rule 62: exp <- exp '+'     %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0063, "rule 63: exp <- exp '-'     %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0064, "rule 64: exp <- exp '*'     %prec MULT_DIV_MOD"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0065, "rule 65: exp <- '*' exp     %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0066, "rule 66: exp <- exp '-' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0067, "rule 67: exp <- exp '*' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0068, "rule 68: exp <- exp '/' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0069, "rule 69: exp <- exp '%' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0070, "rule 70: exp <- exp D exp    %left %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0071, "rule 71: exp <- exp '=' exp    %right %prec ASSIGNMENT"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0072, "rule 72: exp <- exp '^' exp    %right %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0073, "rule 73: exp <- exp OR exp    %left %prec OR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0074, "rule 74: exp <- exp AND exp    %left %prec AND"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0075, "rule 75: exp <- exp EQ exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0076, "rule 76: exp <- exp NE exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0077, "rule 77: exp <- exp LT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0078, "rule 78: exp <- exp GT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0079, "rule 79: exp <- exp LTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0080, "rule 80: exp <- exp GTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0081, "rule 81: exp <- exp CAT exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0082, "rule 82: exp <- '(' exp ')'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0083, "rule 83: exp <- '-' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0084, "rule 84: exp <- '+' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0085, "rule 85: exp <- NOT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0086, "rule 86: exp <- CAT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0087, "rule 87: exp <- NOT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0088, "rule 88: exp <- CAT exp     %prec UNARY"},
    {                  Token::exp__,  4, &SteelParser::ReductionRuleHandler0089, "rule 89: exp <- exp '[' exp ']'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0090, "rule 90: exp <- INCREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0091, "rule 91: exp <- INCREMENT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0092, "rule 92: exp <- %error INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0093, "rule 93: exp <- exp INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0094, "rule 94: exp <- DECREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0095, "rule 95: exp <- DECREMENT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0096, "rule 96: exp <- exp DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0097, "rule 97: exp <- %error DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0098, "rule 98: exp <- POP exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0099, "rule 99: exp <- POP %error     %prec UNARY"},
    {        Token::exp_statement__,  1, &SteelParser::ReductionRuleHandler0100, "rule 100: exp_statement <- ';'    "},
    {        Token::exp_statement__,  2, &SteelParser::ReductionRuleHandler0101, "rule 101: exp_statement <- exp ';'    "},
    {          Token::int_literal__,  1, &SteelParser::ReductionRuleHandler0102, "rule 102: int_literal <- INT    "},
    {       Token::var_identifier__,  1, &SteelParser::ReductionRuleHandler0103, "rule 103: var_identifier <- VAR_IDENTIFIER    "},
    {      Token::func_identifier__,  1, &SteelParser::ReductionRuleHandler0104, "rule 104: func_identifier <- FUNC_IDENTIFIER    "},
    {     Token::array_identifier__,  1, &SteelParser::ReductionRuleHandler0105, "rule 105: array_identifier <- ARRAY_IDENTIFIER    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0106, "rule 106: call <- func_identifier '(' ')'     %prec CORRECT"},
    {                 Token::call__,  4, &SteelParser::ReductionRuleHandler0107, "rule 107: call <- func_identifier '(' param_list ')'     %prec CORRECT"},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0108, "rule 108: call <- func_identifier '(' param_list    "},
    {                 Token::call__,  2, &SteelParser::ReductionRuleHandler0109, "rule 109: call <- func_identifier '('    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0110, "rule 110: call <- func_identifier %error ')'    "},
    {                 Token::call__,  2, &SteelParser::ReductionRuleHandler0111, "rule 111: call <- func_identifier %error    "},
    {                 Token::call__,  1, &SteelParser::ReductionRuleHandler0112, "rule 112: call <- func_identifier    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0113, "rule 113: vardecl <- VAR var_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0114, "rule 114: vardecl <- VAR var_identifier '=' exp    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0115, "rule 115: vardecl <- CONST var_identifier '=' exp    "},
    {              Token::vardecl__,  5, &SteelParser::ReductionRuleHandler0116, "rule 116: vardecl <- VAR array_identifier '[' exp ']'    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0117, "rule 117: vardecl <- VAR array_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0118, "rule 118: vardecl <- VAR array_identifier '=' exp    "},
    {           Token::param_list__,  1, &SteelParser::ReductionRuleHandler0119, "rule 119: param_list <- exp    "},
    {           Token::param_list__,  3, &SteelParser::ReductionRuleHandler0120, "rule 120: param_list <- param_list ',' exp    "},

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
    {   5,   30,    0,   35,    9}, // state    2
    {   0,    0,   44,    0,    0}, // state    3
    {  45,    4,   49,   50,    1}, // state    4
    {   0,    0,   51,    0,    0}, // state    5
    {  52,   16,    0,   68,    5}, // state    6
    {  73,    1,   74,   75,    1}, // state    7
    {  76,   16,    0,   92,    5}, // state    8
    {  97,   16,    0,  113,    5}, // state    9
    { 118,   16,    0,  134,    5}, // state   10
    { 139,   16,    0,  155,    5}, // state   11
    { 160,    2,    0,    0,    0}, // state   12
    { 162,   32,    0,    0,    0}, // state   13
    { 194,   32,    0,    0,    0}, // state   14
    { 226,   32,    0,  258,    5}, // state   15
    { 263,    2,    0,    0,    0}, // state   16
    { 265,    2,    0,    0,    0}, // state   17
    {   0,    0,  267,    0,    0}, // state   18
    {   0,    0,  268,    0,    0}, // state   19
    {   0,    0,  269,    0,    0}, // state   20
    { 270,    2,    0,    0,    0}, // state   21
    { 272,    2,    0,  274,    2}, // state   22
    {   0,    0,  276,    0,    0}, // state   23
    {   0,    0,  277,    0,    0}, // state   24
    {   0,    0,  278,    0,    0}, // state   25
    { 279,   16,    0,  295,    5}, // state   26
    { 300,   16,    0,  316,    5}, // state   27
    { 321,   16,    0,  337,    5}, // state   28
    { 342,   16,    0,  358,    5}, // state   29
    { 363,    1,    0,    0,    0}, // state   30
    { 364,    1,    0,  365,    1}, // state   31
    { 366,   29,    0,  395,    9}, // state   32
    {   0,    0,  404,    0,    0}, // state   33
    {   0,    0,  405,    0,    0}, // state   34
    { 406,   21,    0,    0,    0}, // state   35
    {   0,    0,  427,    0,    0}, // state   36
    {   0,    0,  428,    0,    0}, // state   37
    { 429,   49,    0,    0,    0}, // state   38
    {   0,    0,  478,    0,    0}, // state   39
    {   0,    0,  479,    0,    0}, // state   40
    { 480,    2,    0,    0,    0}, // state   41
    {   0,    0,  482,    0,    0}, // state   42
    {   0,    0,  483,    0,    0}, // state   43
    {   0,    0,  484,    0,    0}, // state   44
    { 485,    2,    0,    0,    0}, // state   45
    { 487,   21,    0,    0,    0}, // state   46
    {   0,    0,  508,    0,    0}, // state   47
    { 509,   30,    0,  539,    9}, // state   48
    { 548,    5,  553,    0,    0}, // state   49
    { 554,    5,  559,    0,    0}, // state   50
    { 560,    8,  568,    0,    0}, // state   51
    { 569,    2,  571,    0,    0}, // state   52
    { 572,    5,  577,    0,    0}, // state   53
    {   0,    0,  578,    0,    0}, // state   54
    { 579,   32,    0,  611,    5}, // state   55
    {   0,    0,  616,    0,    0}, // state   56
    {   0,    0,  617,    0,    0}, // state   57
    {   0,    0,  618,    0,    0}, // state   58
    {   0,    0,  619,    0,    0}, // state   59
    { 620,    2,  622,    0,    0}, // state   60
    {   0,    0,  623,    0,    0}, // state   61
    { 624,   21,    0,    0,    0}, // state   62
    {   0,    0,  645,    0,    0}, // state   63
    { 646,   16,    0,  662,    5}, // state   64
    { 667,    1,    0,    0,    0}, // state   65
    { 668,    1,    0,    0,    0}, // state   66
    {   0,    0,  669,    0,    0}, // state   67
    { 670,   17,    0,  687,    6}, // state   68
    { 693,    1,  694,    0,    0}, // state   69
    { 695,    2,  697,    0,    0}, // state   70
    { 698,    2,  700,    0,    0}, // state   71
    { 701,    5,  706,    0,    0}, // state   72
    { 707,    2,  709,    0,    0}, // state   73
    { 710,    5,  715,    0,    0}, // state   74
    { 716,    2,  718,    0,    0}, // state   75
    { 719,    5,  724,    0,    0}, // state   76
    { 725,    2,  727,    0,    0}, // state   77
    { 728,    5,  733,    0,    0}, // state   78
    { 734,    2,    0,    0,    0}, // state   79
    { 736,    1,    0,    0,    0}, // state   80
    { 737,    2,    0,    0,    0}, // state   81
    {   0,    0,  739,    0,    0}, // state   82
    { 740,   16,    0,  756,    5}, // state   83
    { 761,   16,    0,  777,    5}, // state   84
    { 782,   49,    0,  831,    5}, // state   85
    { 836,   49,    0,  885,    5}, // state   86
    { 890,   49,    0,  939,    5}, // state   87
    { 944,   16,    0,  960,    5}, // state   88
    { 965,   16,    0,  981,    5}, // state   89
    { 986,   16,    0, 1002,    5}, // state   90
    {1007,   16,    0, 1023,    5}, // state   91
    {1028,   16,    0, 1044,    5}, // state   92
    {1049,   16,    0, 1065,    5}, // state   93
    {1070,   16,    0, 1086,    5}, // state   94
    {1091,   16,    0, 1107,    5}, // state   95
    {1112,   16,    0, 1128,    5}, // state   96
    {1133,   16,    0, 1149,    5}, // state   97
    {1154,   16,    0, 1170,    5}, // state   98
    {1175,   16,    0, 1191,    5}, // state   99
    {   0,    0, 1196,    0,    0}, // state  100
    {   0,    0, 1197,    0,    0}, // state  101
    {1198,   16,    0, 1214,    5}, // state  102
    {1219,    1, 1220,    0,    0}, // state  103
    {1221,   49,    0, 1270,    6}, // state  104
    {1276,    1,    0,    0,    0}, // state  105
    {   0,    0, 1277,    0,    0}, // state  106
    {   0,    0, 1278,    0,    0}, // state  107
    {   0,    0, 1279,    0,    0}, // state  108
    {1280,    3,    0,    0,    0}, // state  109
    {1283,   21,    0,    0,    0}, // state  110
    {   0,    0, 1304,    0,    0}, // state  111
    {1305,    3, 1308,    0,    0}, // state  112
    {1309,   21,    0,    0,    0}, // state  113
    {1330,    2, 1332, 1333,    2}, // state  114
    {1335,    5,    0, 1340,    2}, // state  115
    {1342,    2, 1344,    0,    0}, // state  116
    {1345,   17,    0, 1362,    6}, // state  117
    {1368,   16,    0, 1384,    5}, // state  118
    {1389,   16,    0, 1405,    5}, // state  119
    {1410,   16,    0, 1426,    5}, // state  120
    {1431,    1,    0,    0,    0}, // state  121
    {1432,    1,    0,    0,    0}, // state  122
    {1433,   16,    0, 1449,    5}, // state  123
    {   0,    0, 1454,    0,    0}, // state  124
    {1455,    1,    0,    0,    0}, // state  125
    {1456,   20, 1476,    0,    0}, // state  126
    {1477,   21,    0,    0,    0}, // state  127
    {1498,    9, 1507,    0,    0}, // state  128
    {1508,    9, 1517,    0,    0}, // state  129
    {1518,    6, 1524,    0,    0}, // state  130
    {1525,    6, 1531,    0,    0}, // state  131
    {1532,    4, 1536,    0,    0}, // state  132
    {1537,    6, 1543,    0,    0}, // state  133
    {1544,    4, 1548,    0,    0}, // state  134
    {1549,   11, 1560,    0,    0}, // state  135
    {1561,   11, 1572,    0,    0}, // state  136
    {1573,   15, 1588,    0,    0}, // state  137
    {1589,   15, 1604,    0,    0}, // state  138
    {1605,   11, 1616,    0,    0}, // state  139
    {1617,   11, 1628,    0,    0}, // state  140
    {1629,   17, 1646,    0,    0}, // state  141
    {1647,   18, 1665,    0,    0}, // state  142
    {1666,    9, 1675,    0,    0}, // state  143
    {   0,    0, 1676,    0,    0}, // state  144
    {   0,    0, 1677,    0,    0}, // state  145
    {1678,   20, 1698,    0,    0}, // state  146
    {1699,    2, 1701,    0,    0}, // state  147
    {   0,    0, 1702,    0,    0}, // state  148
    {1703,   29,    0, 1732,    9}, // state  149
    {1741,   29,    0, 1770,    9}, // state  150
    {1779,   29,    0, 1808,    9}, // state  151
    {1817,   29,    0, 1846,    9}, // state  152
    {1855,    3,    0,    0,    0}, // state  153
    {   0,    0, 1858,    0,    0}, // state  154
    {1859,    1,    0,    0,    0}, // state  155
    {1860,    3,    0,    0,    0}, // state  156
    {1863,    2, 1865,    0,    0}, // state  157
    {1866,   17,    0, 1883,    5}, // state  158
    {1888,   20, 1908,    0,    0}, // state  159
    {1909,   20, 1929,    0,    0}, // state  160
    {1930,   21,    0,    0,    0}, // state  161
    {1951,    2, 1953, 1954,    2}, // state  162
    {1956,    5,    0, 1961,    2}, // state  163
    {1963,   20, 1983,    0,    0}, // state  164
    {1984,   16,    0, 2000,    5}, // state  165
    {   0,    0, 2005,    0,    0}, // state  166
    {   0,    0, 2006,    0,    0}, // state  167
    {2007,   16,    0, 2023,    5}, // state  168
    {   0,    0, 2028,    0,    0}, // state  169
    {   0,    0, 2029,    0,    0}, // state  170
    {2030,    1, 2031,    0,    0}, // state  171
    {2032,    1, 2033,    0,    0}, // state  172
    {   0,    0, 2034,    0,    0}, // state  173
    {2035,    1,    0,    0,    0}, // state  174
    {2036,    2,    0, 2038,    1}, // state  175
    {2039,    1,    0,    0,    0}, // state  176
    {2040,    1,    0,    0,    0}, // state  177
    {2041,    2, 2043,    0,    0}, // state  178
    {2044,   29,    0, 2073,    9}, // state  179
    {2082,   21, 2103,    0,    0}, // state  180
    {   0,    0, 2104,    0,    0}, // state  181
    {2105,    3,    0,    0,    0}, // state  182
    {2108,    1,    0,    0,    0}, // state  183
    {2109,    3,    0,    0,    0}, // state  184
    {2112,    3, 2115,    0,    0}, // state  185
    {2116,   21,    0,    0,    0}, // state  186
    {2137,   20, 2157,    0,    0}, // state  187
    {2158,   29,    0, 2187,    9}, // state  188
    {2196,   29,    0, 2225,    9}, // state  189
    {   0,    0, 2234, 2235,    1}, // state  190
    {   0,    0, 2236,    0,    0}, // state  191
    {   0,    0, 2237, 2238,    1}, // state  192
    {   0,    0, 2239, 2240,    1}, // state  193
    {   0,    0, 2241,    0,    0}, // state  194
    {2242,   29,    0, 2271,    9}, // state  195
    {2280,    1,    0,    0,    0}, // state  196
    {2281,    1,    0,    0,    0}, // state  197
    {2282,    1,    0,    0,    0}, // state  198
    {   0,    0, 2283,    0,    0}, // state  199
    {   0,    0, 2284,    0,    0}, // state  200
    {   0,    0, 2285,    0,    0}, // state  201
    {   0,    0, 2286,    0,    0}, // state  202
    {2287,   30,    0, 2317,    9}, // state  203
    {2326,   30,    0, 2356,    9}, // state  204
    {2365,   30,    0, 2395,    9}, // state  205
    {   0,    0, 2404,    0,    0}, // state  206
    {   0,    0, 2405, 2406,    1}, // state  207
    {   0,    0, 2407, 2408,    1}, // state  208
    {   0,    0, 2409, 2410,    1}, // state  209
    {   0,    0, 2411,    0,    0}, // state  210
    {   0,    0, 2412,    0,    0}, // state  211
    {   0,    0, 2413,    0,    0}, // state  212
    {2414,   30,    0, 2444,    9}, // state  213
    {2453,   30,    0, 2483,    9}, // state  214
    {2492,   30,    0, 2522,    9}, // state  215
    {   0,    0, 2531,    0,    0}, // state  216
    {   0,    0, 2532,    0,    0}, // state  217
    {   0,    0, 2533,    0,    0}  // state  218

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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,   34}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

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
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   16}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state    5
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  100}},

// ///////////////////////////////////////////////////////////////////////////
// state    6
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   46}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state    7
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,   47}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,   48}},

// ///////////////////////////////////////////////////////////////////////////
// state    8
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state    9
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   50}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   10
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   11
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   52}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   53}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   12
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   54}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   55}},

// ///////////////////////////////////////////////////////////////////////////
// state   13
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   51}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   56}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   57}},
    {              Token::Type('('), {           TA_REDUCE_USING_RULE,   51}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   51}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   51}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,   51}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,   51}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,   51}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,   51}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   51}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   51}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   51}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   51}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   51}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   51}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   51}},
    {        Token::FUNC_IDENTIFIER, {           TA_REDUCE_USING_RULE,   51}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,   51}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,   51}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   51}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   51}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,   51}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,   51}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,   51}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,   51}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,   51}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,   51}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,   51}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   51}},
    {                  Token::CONST, {           TA_REDUCE_USING_RULE,   51}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   51}},

// ///////////////////////////////////////////////////////////////////////////
// state   14
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   54}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   58}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   59}},
    {              Token::Type('('), {           TA_REDUCE_USING_RULE,   54}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   54}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   54}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,   54}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,   54}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,   54}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,   54}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   54}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   54}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   54}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   54}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   54}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   54}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   54}},
    {        Token::FUNC_IDENTIFIER, {           TA_REDUCE_USING_RULE,   54}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,   54}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,   54}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   54}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   54}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,   54}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,   54}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,   54}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,   54}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,   54}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,   54}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,   54}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   54}},
    {                  Token::CONST, {           TA_REDUCE_USING_RULE,   54}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   54}},

// ///////////////////////////////////////////////////////////////////////////
// state   15
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   41}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   41}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   41}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   41}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   41}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   41}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   41}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   41}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   41}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   41}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   41}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   41}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   41}},
    {                  Token::CONST, {           TA_REDUCE_USING_RULE,   41}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   41}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   62}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   16
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   64}},

// ///////////////////////////////////////////////////////////////////////////
// state   17
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   66}},

// ///////////////////////////////////////////////////////////////////////////
// state   18
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  104}},

// ///////////////////////////////////////////////////////////////////////////
// state   19
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state   20
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  105}},

// ///////////////////////////////////////////////////////////////////////////
// state   21
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   68}},

// ///////////////////////////////////////////////////////////////////////////
// state   22
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   69}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   70}},

// ///////////////////////////////////////////////////////////////////////////
// state   23
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   56}},

// ///////////////////////////////////////////////////////////////////////////
// state   24
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   57}},

// ///////////////////////////////////////////////////////////////////////////
// state   25
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   58}},

// ///////////////////////////////////////////////////////////////////////////
// state   26
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   27
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   28
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   29
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   77}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   78}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   30
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state   31
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    // nonterminal transitions
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   32
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,   81}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   33
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   18}},

// ///////////////////////////////////////////////////////////////////////////
// state   34
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   15}},

// ///////////////////////////////////////////////////////////////////////////
// state   35
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state   36
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   17}},

// ///////////////////////////////////////////////////////////////////////////
// state   37
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   59}},

// ///////////////////////////////////////////////////////////////////////////
// state   38
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,  112}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,  112}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,  112}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,  112}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,  112}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,  112}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,  112}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,  112}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,  112}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,  112}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,  112}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,  112}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,  112}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,  112}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,  112}},
    {        Token::FUNC_IDENTIFIER, {           TA_REDUCE_USING_RULE,  112}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,  112}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,  112}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,  112}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,  112}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,  112}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,  112}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,  112}},
    {                  Token::CONST, {           TA_REDUCE_USING_RULE,  112}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,  112}},

// ///////////////////////////////////////////////////////////////////////////
// state   39
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   60}},

// ///////////////////////////////////////////////////////////////////////////
// state   40
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   55}},

// ///////////////////////////////////////////////////////////////////////////
// state   41
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state   42
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   92}},

// ///////////////////////////////////////////////////////////////////////////
// state   43
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   97}},

// ///////////////////////////////////////////////////////////////////////////
// state   44
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},

// ///////////////////////////////////////////////////////////////////////////
// state   45
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state   46
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  107}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state   47
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   20}},

// ///////////////////////////////////////////////////////////////////////////
// state   48
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  108}},
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,   34}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   49
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   83}},

// ///////////////////////////////////////////////////////////////////////////
// state   50
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   84}},

// ///////////////////////////////////////////////////////////////////////////
// state   51
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   65}},

// ///////////////////////////////////////////////////////////////////////////
// state   52
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   85}},

// ///////////////////////////////////////////////////////////////////////////
// state   53
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   87}},

// ///////////////////////////////////////////////////////////////////////////
// state   54
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   26}},

// ///////////////////////////////////////////////////////////////////////////
// state   55
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   25}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  109}},
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
    {                  Token::CONST, {           TA_REDUCE_USING_RULE,   25}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   25}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  110}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   56
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   50}},

// ///////////////////////////////////////////////////////////////////////////
// state   57
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   49}},

// ///////////////////////////////////////////////////////////////////////////
// state   58
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   53}},

// ///////////////////////////////////////////////////////////////////////////
// state   59
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   52}},

// ///////////////////////////////////////////////////////////////////////////
// state   60
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   61
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state   62
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  111}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state   63
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   64
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  112}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  113}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   65
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  114}},

// ///////////////////////////////////////////////////////////////////////////
// state   66
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  115}},

// ///////////////////////////////////////////////////////////////////////////
// state   67
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   48}},

// ///////////////////////////////////////////////////////////////////////////
// state   68
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  116}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  117}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   69
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  118}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  113}},

// ///////////////////////////////////////////////////////////////////////////
// state   70
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  119}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,  120}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  117}},

// ///////////////////////////////////////////////////////////////////////////
// state   71
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   91}},

// ///////////////////////////////////////////////////////////////////////////
// state   72
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   90}},

// ///////////////////////////////////////////////////////////////////////////
// state   73
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   95}},

// ///////////////////////////////////////////////////////////////////////////
// state   74
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   94}},

// ///////////////////////////////////////////////////////////////////////////
// state   75
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   86}},

// ///////////////////////////////////////////////////////////////////////////
// state   76
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   88}},

// ///////////////////////////////////////////////////////////////////////////
// state   77
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   99}},

// ///////////////////////////////////////////////////////////////////////////
// state   78
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state   79
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  121}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,  122}},

// ///////////////////////////////////////////////////////////////////////////
// state   80
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  123}},

// ///////////////////////////////////////////////////////////////////////////
// state   81
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  124}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,  125}},

// ///////////////////////////////////////////////////////////////////////////
// state   82
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  101}},

// ///////////////////////////////////////////////////////////////////////////
// state   83
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   84
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   85
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   63}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   63}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   63}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,   63}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   63}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   63}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,   63}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,   63}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   63}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,   63}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,   63}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,   63}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,   63}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,   63}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,   63}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,   63}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,   63}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,   63}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,   63}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,   63}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,   63}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   63}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   63}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   63}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   63}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   63}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   63}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   63}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   63}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   63}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   63}},
    {                  Token::CONST, {           TA_REDUCE_USING_RULE,   63}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   63}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  128}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   86
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   62}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   62}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   62}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,   62}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   62}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   62}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,   62}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,   62}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,   62}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,   62}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,   62}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,   62}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,   62}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,   62}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,   62}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,   62}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,   62}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,   62}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,   62}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,   62}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   62}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   62}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   62}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   62}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   62}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   62}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   62}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   62}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   62}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   62}},
    {                  Token::CONST, {           TA_REDUCE_USING_RULE,   62}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   62}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  129}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   87
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   64}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   64}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   64}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,   64}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   64}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   64}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,   64}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,   64}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   64}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,   64}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,   64}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,   64}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,   64}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,   64}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,   64}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,   64}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,   64}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,   64}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,   64}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,   64}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,   64}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   64}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   64}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   64}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   64}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   64}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   64}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   64}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   64}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   64}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   64}},
    {                  Token::CONST, {           TA_REDUCE_USING_RULE,   64}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   64}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  130}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   88
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   89
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   90
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   91
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   92
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   93
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   94
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  137}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   95
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  138}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   96
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  139}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   97
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  140}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   98
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  141}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   99
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  142}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  100
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   93}},

// ///////////////////////////////////////////////////////////////////////////
// state  101
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state  102
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  143}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  103
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  144}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  111}},

// ///////////////////////////////////////////////////////////////////////////
// state  104
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,  109}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,  109}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  145}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,  109}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,  109}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,  109}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,  109}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,  109}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,  109}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,  109}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,  109}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,  109}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,  109}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,  109}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,  109}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,  109}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,  109}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,  109}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,  109}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,  109}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,  109}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,  109}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,  109}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,  109}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,  109}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,  109}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,  109}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,  109}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,  109}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,  109}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,  109}},
    {                  Token::CONST, {           TA_REDUCE_USING_RULE,  109}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,  109}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  146}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {           Token::param_list__, {                  TA_PUSH_STATE,  147}},

// ///////////////////////////////////////////////////////////////////////////
// state  105
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  148}},

// ///////////////////////////////////////////////////////////////////////////
// state  106
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   21}},

// ///////////////////////////////////////////////////////////////////////////
// state  107
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   82}},

// ///////////////////////////////////////////////////////////////////////////
// state  108
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   19}},

// ///////////////////////////////////////////////////////////////////////////
// state  109
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  149}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  110
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  150}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state  111
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  112
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  151}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state  113
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  152}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state  114
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  153}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  154}},

// ///////////////////////////////////////////////////////////////////////////
// state  115
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  155}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   10}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   10}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  156}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  154}},

// ///////////////////////////////////////////////////////////////////////////
// state  116
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state  117
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  157}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  158}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  118
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  159}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  119
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  160}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  120
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  161}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  121
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  162}},

// ///////////////////////////////////////////////////////////////////////////
// state  122
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  163}},

// ///////////////////////////////////////////////////////////////////////////
// state  123
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  164}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  124
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   30}},

// ///////////////////////////////////////////////////////////////////////////
// state  125
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  165}},

// ///////////////////////////////////////////////////////////////////////////
// state  126
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   71}},

// ///////////////////////////////////////////////////////////////////////////
// state  127
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  166}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state  128
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   66}},

// ///////////////////////////////////////////////////////////////////////////
// state  129
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   61}},

// ///////////////////////////////////////////////////////////////////////////
// state  130
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   67}},

// ///////////////////////////////////////////////////////////////////////////
// state  131
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   68}},

// ///////////////////////////////////////////////////////////////////////////
// state  132
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   72}},

// ///////////////////////////////////////////////////////////////////////////
// state  133
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   69}},

// ///////////////////////////////////////////////////////////////////////////
// state  134
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   70}},

// ///////////////////////////////////////////////////////////////////////////
// state  135
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   78}},

// ///////////////////////////////////////////////////////////////////////////
// state  136
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   77}},

// ///////////////////////////////////////////////////////////////////////////
// state  137
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   75}},

// ///////////////////////////////////////////////////////////////////////////
// state  138
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   76}},

// ///////////////////////////////////////////////////////////////////////////
// state  139
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state  140
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state  141
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   74}},

// ///////////////////////////////////////////////////////////////////////////
// state  142
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   73}},

// ///////////////////////////////////////////////////////////////////////////
// state  143
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   81}},

// ///////////////////////////////////////////////////////////////////////////
// state  144
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  110}},

// ///////////////////////////////////////////////////////////////////////////
// state  145
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state  146
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  119}},

// ///////////////////////////////////////////////////////////////////////////
// state  147
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  167}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  168}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  108}},

// ///////////////////////////////////////////////////////////////////////////
// state  148
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   23}},

// ///////////////////////////////////////////////////////////////////////////
// state  149
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,  169}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  150
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,  170}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  151
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,  171}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  152
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,  172}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  153
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  173}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  174}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  175}},

// ///////////////////////////////////////////////////////////////////////////
// state  154
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},

// ///////////////////////////////////////////////////////////////////////////
// state  155
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  176}},

// ///////////////////////////////////////////////////////////////////////////
// state  156
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  173}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  177}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  175}},

// ///////////////////////////////////////////////////////////////////////////
// state  157
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state  158
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  178}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  179}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  180}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  159
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  114}},

// ///////////////////////////////////////////////////////////////////////////
// state  160
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  118}},

// ///////////////////////////////////////////////////////////////////////////
// state  161
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  181}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state  162
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  182}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  154}},

// ///////////////////////////////////////////////////////////////////////////
// state  163
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  183}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   10}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   10}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  184}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  154}},

// ///////////////////////////////////////////////////////////////////////////
// state  164
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  115}},

// ///////////////////////////////////////////////////////////////////////////
// state  165
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  185}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  186}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  166
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   89}},

// ///////////////////////////////////////////////////////////////////////////
// state  167
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  107}},

// ///////////////////////////////////////////////////////////////////////////
// state  168
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  187}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  169
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   27}},

// ///////////////////////////////////////////////////////////////////////////
// state  170
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   24}},

// ///////////////////////////////////////////////////////////////////////////
// state  171
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  188}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   35}},

// ///////////////////////////////////////////////////////////////////////////
// state  172
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  189}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   33}},

// ///////////////////////////////////////////////////////////////////////////
// state  173
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},

// ///////////////////////////////////////////////////////////////////////////
// state  174
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  190}},

// ///////////////////////////////////////////////////////////////////////////
// state  175
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,  191}},

// ///////////////////////////////////////////////////////////////////////////
// state  176
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  192}},

// ///////////////////////////////////////////////////////////////////////////
// state  177
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  193}},

// ///////////////////////////////////////////////////////////////////////////
// state  178
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   46}},

// ///////////////////////////////////////////////////////////////////////////
// state  179
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,  194}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  180
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  195}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   47}},

// ///////////////////////////////////////////////////////////////////////////
// state  181
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  116}},

// ///////////////////////////////////////////////////////////////////////////
// state  182
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  173}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  196}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  175}},

// ///////////////////////////////////////////////////////////////////////////
// state  183
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  197}},

// ///////////////////////////////////////////////////////////////////////////
// state  184
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  173}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  198}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  175}},

// ///////////////////////////////////////////////////////////////////////////
// state  185
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  199}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   42}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   31}},

// ///////////////////////////////////////////////////////////////////////////
// state  186
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  200}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state  187
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  120}},

// ///////////////////////////////////////////////////////////////////////////
// state  188
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,  201}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  189
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,  202}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  190
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  203}},

// ///////////////////////////////////////////////////////////////////////////
// state  191
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   12}},

// ///////////////////////////////////////////////////////////////////////////
// state  192
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  204}},

// ///////////////////////////////////////////////////////////////////////////
// state  193
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  205}},

// ///////////////////////////////////////////////////////////////////////////
// state  194
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  195
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,  206}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  196
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  207}},

// ///////////////////////////////////////////////////////////////////////////
// state  197
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  208}},

// ///////////////////////////////////////////////////////////////////////////
// state  198
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  209}},

// ///////////////////////////////////////////////////////////////////////////
// state  199
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   29}},

// ///////////////////////////////////////////////////////////////////////////
// state  200
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   28}},

// ///////////////////////////////////////////////////////////////////////////
// state  201
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   34}},

// ///////////////////////////////////////////////////////////////////////////
// state  202
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   32}},

// ///////////////////////////////////////////////////////////////////////////
// state  203
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  210}},
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,   34}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  204
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  211}},
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,   34}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  205
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  212}},
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,   34}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  206
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  207
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  213}},

// ///////////////////////////////////////////////////////////////////////////
// state  208
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  214}},

// ///////////////////////////////////////////////////////////////////////////
// state  209
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  215}},

// ///////////////////////////////////////////////////////////////////////////
// state  210
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    5}},

// ///////////////////////////////////////////////////////////////////////////
// state  211
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    3}},

// ///////////////////////////////////////////////////////////////////////////
// state  212
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    2}},

// ///////////////////////////////////////////////////////////////////////////
// state  213
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  216}},
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,   34}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  214
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  217}},
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,   34}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  215
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  218}},
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
    {                  Token::CONST, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   33}},
    {            Token::statement__, {                  TA_PUSH_STATE,   34}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   35}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   36}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   37}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   38}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   39}},
    {                 Token::call__, {                  TA_PUSH_STATE,   40}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  216
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    6}},

// ///////////////////////////////////////////////////////////////////////////
// state  217
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    4}},

// ///////////////////////////////////////////////////////////////////////////
// state  218
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

#line 6264 "SteelParser.cpp"

