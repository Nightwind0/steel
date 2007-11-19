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
        "BOOLEAN",
        "BREAK",
        "CAT",
        "CONSTANT",
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

#line 137 "steel.trison"

				AstScript * pScript =   new AstScript(
							list->GetLine(),
							list->GetScript());
		        pScript->SetList(list);
				return pScript;
			
#line 511 "SteelParser.cpp"
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

#line 150 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									false);
				
#line 533 "SteelParser.cpp"
}

// rule 3: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' %error ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0003 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 160 "steel.trison"

					AstFuncIdentifier *pId = static_cast<AstFuncIdentifier*>(id);
					addError(id->GetLine(),"parser error in parameter list for function '" + pId->getValue() + '\'');
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									pId,
									NULL,
									stmts,
									false);
				
#line 555 "SteelParser.cpp"
}

// rule 4: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' %error ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0004 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 172 "steel.trison"

					AstFuncIdentifier *pId = static_cast<AstFuncIdentifier*>(id);
					addError(id->GetLine(),"parser error in parameter list for function '" + pId->getValue() + '\'');
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									pId,
									NULL,
									stmts,
									true);
				
#line 577 "SteelParser.cpp"
}

// rule 5: func_definition <- FUNCTION %error '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0005 ()
{
    assert(3 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 184 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									params,
									stmts,
									false);
				
#line 600 "SteelParser.cpp"
}

// rule 6: func_definition <- FINAL FUNCTION %error '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0006 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 197 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									params,
									stmts,
									true);
				
#line 623 "SteelParser.cpp"
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

#line 211 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									true);
				
#line 645 "SteelParser.cpp"
}

// rule 8: param_id <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0008 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 225 "steel.trison"
 return id; 
#line 656 "SteelParser.cpp"
}

// rule 9: param_id <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0009 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 227 "steel.trison"
 return id; 
#line 667 "SteelParser.cpp"
}

// rule 10: param_definition <-     
AstBase* SteelParser::ReductionRuleHandler0010 ()
{

#line 232 "steel.trison"

		 return new AstParamDefinitionList(GET_LINE(), GET_SCRIPT());
	
#line 678 "SteelParser.cpp"
}

// rule 11: param_definition <- vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0011 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 237 "steel.trison"

				AstParamDefinitionList * pList = new AstParamDefinitionList(decl->GetLine(),decl->GetScript());
				pList->add(static_cast<AstDeclaration*>(decl));
				return pList;		
			
#line 693 "SteelParser.cpp"
}

// rule 12: param_definition <- param_definition:list ',' vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0012 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamDefinitionList* list = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 244 "steel.trison"

				list->add(static_cast<AstDeclaration*>(decl));
				return list;
			
#line 709 "SteelParser.cpp"
}

// rule 13: param_definition <- param_definition:list %error    
AstBase* SteelParser::ReductionRuleHandler0013 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamDefinitionList* list = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 250 "steel.trison"

				addError(list->GetLine(),"expected parameter definition");
				return list;
			
#line 723 "SteelParser.cpp"
}

// rule 14: statement_list <-     
AstBase* SteelParser::ReductionRuleHandler0014 ()
{

#line 259 "steel.trison"

				AstStatementList *pList = 
					new AstStatementList(m_scanner->getCurrentLine(),
										m_scanner->getScriptName());
				return pList;
			
#line 737 "SteelParser.cpp"
}

// rule 15: statement_list <- statement_list:list statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0015 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 267 "steel.trison"

					list->add( stmt ); 
					return list;
				
#line 753 "SteelParser.cpp"
}

// rule 16: statement <- %error    
AstBase* SteelParser::ReductionRuleHandler0016 ()
{

#line 275 "steel.trison"
 
			addError(GET_LINE(),"parse error");
			return new AstStatement(GET_LINE(),GET_SCRIPT());
		
#line 765 "SteelParser.cpp"
}

// rule 17: statement <- exp_statement:exp    
AstBase* SteelParser::ReductionRuleHandler0017 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 280 "steel.trison"
 return new AstExpressionStatement(exp->GetLine(),exp->GetScript(), exp); 
#line 776 "SteelParser.cpp"
}

// rule 18: statement <- func_definition:func    
AstBase* SteelParser::ReductionRuleHandler0018 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFunctionDefinition* func = static_cast< AstFunctionDefinition* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 282 "steel.trison"
 return func; 
#line 787 "SteelParser.cpp"
}

// rule 19: statement <- '{' statement_list:list '}'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0019 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 284 "steel.trison"
 return list; 
#line 798 "SteelParser.cpp"
}

// rule 20: statement <- '{' '}'    
AstBase* SteelParser::ReductionRuleHandler0020 ()
{

#line 286 "steel.trison"

			 return new AstStatement(GET_LINE(),GET_SCRIPT());
			
#line 809 "SteelParser.cpp"
}

// rule 21: statement <- vardecl:vardecl ';'    
AstBase* SteelParser::ReductionRuleHandler0021 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* vardecl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 290 "steel.trison"
 return vardecl; 
#line 820 "SteelParser.cpp"
}

// rule 22: statement <- %error vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0022 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 293 "steel.trison"

				addError(decl->GetLine(),"unexpected tokens found before variable declaration.");
				return decl;
			
#line 834 "SteelParser.cpp"
}

// rule 23: statement <- vardecl:decl %error ';'    
AstBase* SteelParser::ReductionRuleHandler0023 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 299 "steel.trison"

			addError(decl->GetLine(),"expected ';' after variable declaration.");
			return decl;
		
#line 848 "SteelParser.cpp"
}

// rule 24: statement <- WHILE '(' exp:exp ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0024 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 304 "steel.trison"
 return new AstWhileStatement(exp->GetLine(), exp->GetScript(),exp,stmt); 
#line 861 "SteelParser.cpp"
}

// rule 25: statement <- WHILE '('    
AstBase* SteelParser::ReductionRuleHandler0025 ()
{

#line 307 "steel.trison"
 
				addError(GET_LINE(),"expected ')'");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT())); 
			
#line 874 "SteelParser.cpp"
}

// rule 26: statement <- WHILE %error    
AstBase* SteelParser::ReductionRuleHandler0026 ()
{

#line 314 "steel.trison"
 
				addError(GET_LINE(),"missing loop condition.");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT())); 
			
#line 887 "SteelParser.cpp"
}

// rule 27: statement <- WHILE '(' %error ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0027 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 322 "steel.trison"
 
				addError(GET_LINE(),"error in loop expression.");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt);    
			
#line 901 "SteelParser.cpp"
}

// rule 28: statement <- DO statement:stmt WHILE '(' exp:condition ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0028 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 328 "steel.trison"

				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), condition, stmt);
	   
#line 916 "SteelParser.cpp"
}

// rule 29: statement <- DO statement:stmt WHILE '(' %error ')'    
AstBase* SteelParser::ReductionRuleHandler0029 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 333 "steel.trison"

				addError(GET_LINE(),"error in while condition.");
				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), NULL, stmt);
	   
#line 930 "SteelParser.cpp"
}

// rule 30: statement <- DO statement:stmt %error    
AstBase* SteelParser::ReductionRuleHandler0030 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 339 "steel.trison"

				addError(GET_LINE(),"error. do loop missing proper while condition.");
				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), NULL, stmt);
	   
#line 944 "SteelParser.cpp"
}

// rule 31: statement <- DO statement:stmt WHILE '(' %error    
AstBase* SteelParser::ReductionRuleHandler0031 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 346 "steel.trison"

				addError(GET_LINE(),"error, missing condition or no closing ')' found after while.");
				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), NULL, NULL);
	   
#line 958 "SteelParser.cpp"
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

#line 353 "steel.trison"
 return new AstIfStatement(exp->GetLine(), exp->GetScript(),exp,stmt,elses);
#line 973 "SteelParser.cpp"
}

// rule 33: statement <- IF '(' exp:exp ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0033 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 355 "steel.trison"
 return new AstIfStatement(exp->GetLine(),exp->GetScript(),exp,stmt); 
#line 986 "SteelParser.cpp"
}

// rule 34: statement <- IF '(' %error ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0034 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 358 "steel.trison"

			addError(GET_LINE(),"parse error in if condition."); 
			return new AstIfStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt,elses);
		
#line 1002 "SteelParser.cpp"
}

// rule 35: statement <- IF '(' %error ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0035 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 364 "steel.trison"

				addError(GET_LINE(),"parse error in if condition."); 
				return new AstIfStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt);
			
#line 1016 "SteelParser.cpp"
}

// rule 36: statement <- IF '(' %error    
AstBase* SteelParser::ReductionRuleHandler0036 ()
{

#line 370 "steel.trison"

			addError(GET_LINE(),"expected ')' after if condition.");
			return new AstIfStatement(GET_LINE(),GET_SCRIPT(),
				new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT()));
		
#line 1029 "SteelParser.cpp"
}

// rule 37: statement <- IF %error    
AstBase* SteelParser::ReductionRuleHandler0037 ()
{

#line 377 "steel.trison"

			addError(GET_LINE(),"expected opening '(' after 'if'");
			return new AstIfStatement(GET_LINE(),GET_SCRIPT(),
				new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT()));

		
#line 1043 "SteelParser.cpp"
}

// rule 38: statement <- RETURN exp:exp ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0038 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 385 "steel.trison"
 return new AstReturnStatement(exp->GetLine(),exp->GetScript(),exp);
#line 1054 "SteelParser.cpp"
}

// rule 39: statement <- RETURN ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0039 ()
{

#line 388 "steel.trison"

				return new AstReturnStatement(GET_LINE(),GET_SCRIPT());
			
#line 1065 "SteelParser.cpp"
}

// rule 40: statement <- RETURN %error    
AstBase* SteelParser::ReductionRuleHandler0040 ()
{

#line 393 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'return'");
				return new AstReturnStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1077 "SteelParser.cpp"
}

// rule 41: statement <- RETURN    
AstBase* SteelParser::ReductionRuleHandler0041 ()
{

#line 399 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'return'");
				return new AstReturnStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1089 "SteelParser.cpp"
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

#line 406 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition, 
						new AstExpression (start->GetLine(),start->GetScript()), stmt); 
			
#line 1107 "SteelParser.cpp"
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

#line 412 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition,iteration,stmt);
			
#line 1126 "SteelParser.cpp"
}

// rule 44: statement <- FOR '(' %error    
AstBase* SteelParser::ReductionRuleHandler0044 ()
{

#line 417 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1140 "SteelParser.cpp"
}

// rule 45: statement <- FOR '(' exp_statement:start %error    
AstBase* SteelParser::ReductionRuleHandler0045 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 425 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1156 "SteelParser.cpp"
}

// rule 46: statement <- FOR '(' exp_statement:start exp_statement:condition %error    
AstBase* SteelParser::ReductionRuleHandler0046 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 433 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1174 "SteelParser.cpp"
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

#line 441 "steel.trison"

				addError(GET_LINE(),"malformed for loop. Expected ')'");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1194 "SteelParser.cpp"
}

// rule 48: statement <- FOR %error    
AstBase* SteelParser::ReductionRuleHandler0048 ()
{

#line 449 "steel.trison"

				addError(GET_LINE(),"malformed for loop. Expected opening '('");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1208 "SteelParser.cpp"
}

// rule 49: statement <- BREAK ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0049 ()
{

#line 458 "steel.trison"

				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1219 "SteelParser.cpp"
}

// rule 50: statement <- BREAK %error    
AstBase* SteelParser::ReductionRuleHandler0050 ()
{

#line 463 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'break'");
				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1231 "SteelParser.cpp"
}

// rule 51: statement <- BREAK    
AstBase* SteelParser::ReductionRuleHandler0051 ()
{

#line 469 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'break'");
				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1243 "SteelParser.cpp"
}

// rule 52: statement <- CONTINUE ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0052 ()
{

#line 477 "steel.trison"

				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1254 "SteelParser.cpp"
}

// rule 53: statement <- CONTINUE %error    
AstBase* SteelParser::ReductionRuleHandler0053 ()
{

#line 482 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'continue'");
				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1266 "SteelParser.cpp"
}

// rule 54: statement <- CONTINUE    
AstBase* SteelParser::ReductionRuleHandler0054 ()
{

#line 488 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'continue'");
				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1278 "SteelParser.cpp"
}

// rule 55: exp <- call:call    
AstBase* SteelParser::ReductionRuleHandler0055 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstCallExpression* call = static_cast< AstCallExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 499 "steel.trison"
 return call; 
#line 1289 "SteelParser.cpp"
}

// rule 56: exp <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0056 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 501 "steel.trison"
 return i;
#line 1300 "SteelParser.cpp"
}

// rule 57: exp <- FLOAT:f    
AstBase* SteelParser::ReductionRuleHandler0057 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* f = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 503 "steel.trison"
 return f; 
#line 1311 "SteelParser.cpp"
}

// rule 58: exp <- STRING:s    
AstBase* SteelParser::ReductionRuleHandler0058 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* s = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 505 "steel.trison"
 return s; 
#line 1322 "SteelParser.cpp"
}

// rule 59: exp <- BOOLEAN:b    
AstBase* SteelParser::ReductionRuleHandler0059 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* b = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 507 "steel.trison"
 return b; 
#line 1333 "SteelParser.cpp"
}

// rule 60: exp <- var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0060 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 509 "steel.trison"
 return id; 
#line 1344 "SteelParser.cpp"
}

// rule 61: exp <- array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0061 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 511 "steel.trison"
 return id; 
#line 1355 "SteelParser.cpp"
}

// rule 62: exp <- exp:a '+' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0062 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 513 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,b); 
#line 1368 "SteelParser.cpp"
}

// rule 63: exp <- exp:a '+'     %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0063 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 515 "steel.trison"
 
				addError(a->GetLine(),"expected expression before '+'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1382 "SteelParser.cpp"
}

// rule 64: exp <- exp:a '-'     %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0064 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 520 "steel.trison"
 
				addError(a->GetLine(),"expected expression before '-'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1396 "SteelParser.cpp"
}

// rule 65: exp <- exp:a '*'     %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0065 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 525 "steel.trison"
 
				addError(a->GetLine(),"expected expression after '*'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1410 "SteelParser.cpp"
}

// rule 66: exp <- '*' exp:b     %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0066 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 530 "steel.trison"
 
				addError(b->GetLine(),"expected expression before '*'.");	
				return new AstBinOp(b->GetLine(),b->GetScript(),AstBinOp::MULT,new AstExpression(GET_LINE(),GET_SCRIPT()),b);
			  
#line 1424 "SteelParser.cpp"
}

// rule 67: exp <- exp:a '-' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0067 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 536 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,b); 
#line 1437 "SteelParser.cpp"
}

// rule 68: exp <- exp:a '*' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0068 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 538 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,b); 
#line 1450 "SteelParser.cpp"
}

// rule 69: exp <- exp:a '/' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0069 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 540 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::DIV,a,b); 
#line 1463 "SteelParser.cpp"
}

// rule 70: exp <- exp:a '%' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0070 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 542 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MOD,a,b); 
#line 1476 "SteelParser.cpp"
}

// rule 71: exp <- exp:a D exp:b    %left %prec POW
AstBase* SteelParser::ReductionRuleHandler0071 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 544 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::D,a,b); 
#line 1489 "SteelParser.cpp"
}

// rule 72: exp <- exp:lvalue '=' exp:exp    %right %prec ASSIGNMENT
AstBase* SteelParser::ReductionRuleHandler0072 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 546 "steel.trison"
 return new AstVarAssignmentExpression(lvalue->GetLine(),lvalue->GetScript(),lvalue,exp); 
#line 1502 "SteelParser.cpp"
}

// rule 73: exp <- exp:a '^' exp:b    %right %prec POW
AstBase* SteelParser::ReductionRuleHandler0073 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 548 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::POW,a,b); 
#line 1515 "SteelParser.cpp"
}

// rule 74: exp <- exp:a OR exp:b    %left %prec OR
AstBase* SteelParser::ReductionRuleHandler0074 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 550 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::OR,a,b); 
#line 1528 "SteelParser.cpp"
}

// rule 75: exp <- exp:a AND exp:b    %left %prec AND
AstBase* SteelParser::ReductionRuleHandler0075 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 552 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::AND,a,b); 
#line 1541 "SteelParser.cpp"
}

// rule 76: exp <- exp:a EQ exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0076 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 554 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::EQ,a,b); 
#line 1554 "SteelParser.cpp"
}

// rule 77: exp <- exp:a NE exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0077 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 556 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::NE,a,b); 
#line 1567 "SteelParser.cpp"
}

// rule 78: exp <- exp:a LT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0078 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 558 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LT,a,b); 
#line 1580 "SteelParser.cpp"
}

// rule 79: exp <- exp:a GT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0079 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 560 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GT,a,b); 
#line 1593 "SteelParser.cpp"
}

// rule 80: exp <- exp:a LTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0080 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 562 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LTE,a,b); 
#line 1606 "SteelParser.cpp"
}

// rule 81: exp <- exp:a GTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0081 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 564 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GTE,a,b); 
#line 1619 "SteelParser.cpp"
}

// rule 82: exp <- exp:a CAT exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0082 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 566 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::CAT,a,b); 
#line 1632 "SteelParser.cpp"
}

// rule 83: exp <- '(' exp:exp ')'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0083 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 568 "steel.trison"
 return exp; 
#line 1643 "SteelParser.cpp"
}

// rule 84: exp <- '-' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0084 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 570 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::MINUS,exp); 
#line 1654 "SteelParser.cpp"
}

// rule 85: exp <- '+' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0085 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 572 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::PLUS,exp); 
#line 1665 "SteelParser.cpp"
}

// rule 86: exp <- NOT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0086 ()
{

#line 574 "steel.trison"

						addError(GET_LINE(),"expected expression after unary minus.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::NOT,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1677 "SteelParser.cpp"
}

// rule 87: exp <- CAT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0087 ()
{

#line 582 "steel.trison"

						addError(GET_LINE(),"expected expression after ':'.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::CAT,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1689 "SteelParser.cpp"
}

// rule 88: exp <- NOT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0088 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 588 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::NOT,exp); 
#line 1700 "SteelParser.cpp"
}

// rule 89: exp <- CAT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0089 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 590 "steel.trison"
 return new AstUnaryOp(exp->GetLine(),
exp->GetScript(), AstUnaryOp::CAT,exp); 
#line 1712 "SteelParser.cpp"
}

// rule 90: exp <- exp:lvalue '[' exp:index ']'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0090 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* index = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 593 "steel.trison"
 return new AstArrayElement(lvalue->GetLine(),lvalue->GetScript(),lvalue,index); 
#line 1725 "SteelParser.cpp"
}

// rule 91: exp <- INCREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0091 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 595 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::PRE);
#line 1736 "SteelParser.cpp"
}

// rule 92: exp <- INCREMENT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0092 ()
{

#line 597 "steel.trison"

										addError(GET_LINE(),"expected lvalue after '++'");
										return new AstIncrement(GET_LINE(),GET_SCRIPT(),
												new AstExpression(GET_LINE(),GET_SCRIPT()),AstIncrement::PRE);
										
#line 1749 "SteelParser.cpp"
}

// rule 93: exp <- %error INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0093 ()
{

#line 603 "steel.trison"
 
									addError(GET_LINE(),"expected lvalue before '++'");
									return new AstIncrement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()), AstIncrement::POST);
	   							
#line 1761 "SteelParser.cpp"
}

// rule 94: exp <- exp:lvalue INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0094 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 609 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::POST);
#line 1772 "SteelParser.cpp"
}

// rule 95: exp <- DECREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0095 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 611 "steel.trison"
 return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::PRE);
#line 1783 "SteelParser.cpp"
}

// rule 96: exp <- DECREMENT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0096 ()
{

#line 613 "steel.trison"

										addError(GET_LINE(),"expected lvalue after '--'");
										return new AstDecrement(GET_LINE(),GET_SCRIPT(),
												new AstExpression(GET_LINE(),GET_SCRIPT()),AstDecrement::PRE);
										
#line 1796 "SteelParser.cpp"
}

// rule 97: exp <- exp:lvalue DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0097 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 620 "steel.trison"
 
									return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::POST);
									
#line 1809 "SteelParser.cpp"
}

// rule 98: exp <- %error DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0098 ()
{

#line 624 "steel.trison"
 
									addError(GET_LINE(),"expected lvalue before '--'");
									return new AstDecrement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()), AstDecrement::POST);
									
#line 1821 "SteelParser.cpp"
}

// rule 99: exp <- POP exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0099 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 630 "steel.trison"
 return new AstPop(lvalue->GetLine(),lvalue->GetScript(),lvalue); 
#line 1832 "SteelParser.cpp"
}

// rule 100: exp <- POP %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0100 ()
{

#line 632 "steel.trison"

						addError(GET_LINE(),"expected expression after 'pop'.");
						return new AstPop(GET_LINE(),GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()));
							  
#line 1844 "SteelParser.cpp"
}

// rule 101: exp_statement <- ';'    
AstBase* SteelParser::ReductionRuleHandler0101 ()
{

#line 641 "steel.trison"

			return new AstExpression(GET_LINE(),GET_SCRIPT()); 
		
#line 1855 "SteelParser.cpp"
}

// rule 102: exp_statement <- exp:exp ';'    
AstBase* SteelParser::ReductionRuleHandler0102 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 645 "steel.trison"
 return exp; 
#line 1866 "SteelParser.cpp"
}

// rule 103: int_literal <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0103 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 650 "steel.trison"
 return i; 
#line 1877 "SteelParser.cpp"
}

// rule 104: var_identifier <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0104 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 655 "steel.trison"
 return id; 
#line 1888 "SteelParser.cpp"
}

// rule 105: func_identifier <- FUNC_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0105 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 660 "steel.trison"
 return id; 
#line 1899 "SteelParser.cpp"
}

// rule 106: array_identifier <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0106 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 665 "steel.trison"
 return id; 
#line 1910 "SteelParser.cpp"
}

// rule 107: call <- func_identifier:id '(' ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0107 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 671 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id);
#line 1921 "SteelParser.cpp"
}

// rule 108: call <- func_identifier:id '(' param_list:params ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0108 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 673 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id,params);
#line 1934 "SteelParser.cpp"
}

// rule 109: call <- func_identifier:id '(' param_list:params    
AstBase* SteelParser::ReductionRuleHandler0109 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 676 "steel.trison"

				addError(GET_LINE(),"expected ')' before ';'.");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id,params); 
			
#line 1950 "SteelParser.cpp"
}

// rule 110: call <- func_identifier:id '('    
AstBase* SteelParser::ReductionRuleHandler0110 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 682 "steel.trison"

				addError(GET_LINE(),"expected ')' before ';'");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1964 "SteelParser.cpp"
}

// rule 111: call <- func_identifier:id %error ')'    
AstBase* SteelParser::ReductionRuleHandler0111 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 688 "steel.trison"

				addError(GET_LINE(),"missing '(' in function call");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1978 "SteelParser.cpp"
}

// rule 112: call <- func_identifier:id %error    
AstBase* SteelParser::ReductionRuleHandler0112 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 694 "steel.trison"

				addError(GET_LINE(),"function call missing parentheses.");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1992 "SteelParser.cpp"
}

// rule 113: call <- func_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0113 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 700 "steel.trison"

				addError(GET_LINE(),"invalid bareword. function call missing parentheses?");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 2006 "SteelParser.cpp"
}

// rule 114: vardecl <- VAR var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0114 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 709 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id);
#line 2017 "SteelParser.cpp"
}

// rule 115: vardecl <- VAR var_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0115 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 711 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id,exp); 
#line 2030 "SteelParser.cpp"
}

// rule 116: vardecl <- CONSTANT var_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0116 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 713 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id,true,exp); 
#line 2043 "SteelParser.cpp"
}

// rule 117: vardecl <- VAR array_identifier:id '[' exp:i ']'    
AstBase* SteelParser::ReductionRuleHandler0117 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* i = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 715 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id,i); 
#line 2056 "SteelParser.cpp"
}

// rule 118: vardecl <- VAR array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0118 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 717 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id); 
#line 2067 "SteelParser.cpp"
}

// rule 119: vardecl <- VAR array_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0119 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 719 "steel.trison"

							AstArrayDeclaration *pDecl =  new AstArrayDeclaration(id->GetLine(),id->GetScript(),id);
							pDecl->assign(exp);
							return pDecl;
						 
#line 2084 "SteelParser.cpp"
}

// rule 120: param_list <- exp:exp    
AstBase* SteelParser::ReductionRuleHandler0120 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 728 "steel.trison"
 AstParamList * pList = new AstParamList ( exp->GetLine(), exp->GetScript() );
		  pList->add(exp);
		  return pList;
		
#line 2098 "SteelParser.cpp"
}

// rule 121: param_list <- param_list:list ',' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0121 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamList* list = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 733 "steel.trison"
 list->add(exp); return list;
#line 2111 "SteelParser.cpp"
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
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0059, "rule 59: exp <- BOOLEAN    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0060, "rule 60: exp <- var_identifier    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0061, "rule 61: exp <- array_identifier    "},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0062, "rule 62: exp <- exp '+' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0063, "rule 63: exp <- exp '+'     %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0064, "rule 64: exp <- exp '-'     %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0065, "rule 65: exp <- exp '*'     %prec MULT_DIV_MOD"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0066, "rule 66: exp <- '*' exp     %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0067, "rule 67: exp <- exp '-' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0068, "rule 68: exp <- exp '*' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0069, "rule 69: exp <- exp '/' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0070, "rule 70: exp <- exp '%' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0071, "rule 71: exp <- exp D exp    %left %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0072, "rule 72: exp <- exp '=' exp    %right %prec ASSIGNMENT"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0073, "rule 73: exp <- exp '^' exp    %right %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0074, "rule 74: exp <- exp OR exp    %left %prec OR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0075, "rule 75: exp <- exp AND exp    %left %prec AND"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0076, "rule 76: exp <- exp EQ exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0077, "rule 77: exp <- exp NE exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0078, "rule 78: exp <- exp LT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0079, "rule 79: exp <- exp GT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0080, "rule 80: exp <- exp LTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0081, "rule 81: exp <- exp GTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0082, "rule 82: exp <- exp CAT exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0083, "rule 83: exp <- '(' exp ')'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0084, "rule 84: exp <- '-' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0085, "rule 85: exp <- '+' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0086, "rule 86: exp <- NOT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0087, "rule 87: exp <- CAT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0088, "rule 88: exp <- NOT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0089, "rule 89: exp <- CAT exp     %prec UNARY"},
    {                  Token::exp__,  4, &SteelParser::ReductionRuleHandler0090, "rule 90: exp <- exp '[' exp ']'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0091, "rule 91: exp <- INCREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0092, "rule 92: exp <- INCREMENT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0093, "rule 93: exp <- %error INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0094, "rule 94: exp <- exp INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0095, "rule 95: exp <- DECREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0096, "rule 96: exp <- DECREMENT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0097, "rule 97: exp <- exp DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0098, "rule 98: exp <- %error DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0099, "rule 99: exp <- POP exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0100, "rule 100: exp <- POP %error     %prec UNARY"},
    {        Token::exp_statement__,  1, &SteelParser::ReductionRuleHandler0101, "rule 101: exp_statement <- ';'    "},
    {        Token::exp_statement__,  2, &SteelParser::ReductionRuleHandler0102, "rule 102: exp_statement <- exp ';'    "},
    {          Token::int_literal__,  1, &SteelParser::ReductionRuleHandler0103, "rule 103: int_literal <- INT    "},
    {       Token::var_identifier__,  1, &SteelParser::ReductionRuleHandler0104, "rule 104: var_identifier <- VAR_IDENTIFIER    "},
    {      Token::func_identifier__,  1, &SteelParser::ReductionRuleHandler0105, "rule 105: func_identifier <- FUNC_IDENTIFIER    "},
    {     Token::array_identifier__,  1, &SteelParser::ReductionRuleHandler0106, "rule 106: array_identifier <- ARRAY_IDENTIFIER    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0107, "rule 107: call <- func_identifier '(' ')'     %prec CORRECT"},
    {                 Token::call__,  4, &SteelParser::ReductionRuleHandler0108, "rule 108: call <- func_identifier '(' param_list ')'     %prec CORRECT"},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0109, "rule 109: call <- func_identifier '(' param_list    "},
    {                 Token::call__,  2, &SteelParser::ReductionRuleHandler0110, "rule 110: call <- func_identifier '('    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0111, "rule 111: call <- func_identifier %error ')'    "},
    {                 Token::call__,  2, &SteelParser::ReductionRuleHandler0112, "rule 112: call <- func_identifier %error    "},
    {                 Token::call__,  1, &SteelParser::ReductionRuleHandler0113, "rule 113: call <- func_identifier    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0114, "rule 114: vardecl <- VAR var_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0115, "rule 115: vardecl <- VAR var_identifier '=' exp    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0116, "rule 116: vardecl <- CONSTANT var_identifier '=' exp    "},
    {              Token::vardecl__,  5, &SteelParser::ReductionRuleHandler0117, "rule 117: vardecl <- VAR array_identifier '[' exp ']'    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0118, "rule 118: vardecl <- VAR array_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0119, "rule 119: vardecl <- VAR array_identifier '=' exp    "},
    {           Token::param_list__,  1, &SteelParser::ReductionRuleHandler0120, "rule 120: param_list <- exp    "},
    {           Token::param_list__,  3, &SteelParser::ReductionRuleHandler0121, "rule 121: param_list <- param_list ',' exp    "},

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
    {   5,   31,    0,   36,    9}, // state    2
    {   0,    0,   45,    0,    0}, // state    3
    {  46,    4,   50,   51,    1}, // state    4
    {   0,    0,   52,    0,    0}, // state    5
    {  53,   17,    0,   70,    5}, // state    6
    {  75,    1,   76,   77,    1}, // state    7
    {  78,   17,    0,   95,    5}, // state    8
    { 100,   17,    0,  117,    5}, // state    9
    { 122,   17,    0,  139,    5}, // state   10
    { 144,   17,    0,  161,    5}, // state   11
    { 166,    2,    0,    0,    0}, // state   12
    { 168,   33,    0,    0,    0}, // state   13
    { 201,   33,    0,    0,    0}, // state   14
    { 234,   33,    0,  267,    5}, // state   15
    { 272,    2,    0,    0,    0}, // state   16
    { 274,    2,    0,    0,    0}, // state   17
    {   0,    0,  276,    0,    0}, // state   18
    {   0,    0,  277,    0,    0}, // state   19
    {   0,    0,  278,    0,    0}, // state   20
    { 279,    2,    0,    0,    0}, // state   21
    { 281,    2,    0,  283,    2}, // state   22
    {   0,    0,  285,    0,    0}, // state   23
    {   0,    0,  286,    0,    0}, // state   24
    {   0,    0,  287,    0,    0}, // state   25
    {   0,    0,  288,    0,    0}, // state   26
    { 289,   17,    0,  306,    5}, // state   27
    { 311,   17,    0,  328,    5}, // state   28
    { 333,   17,    0,  350,    5}, // state   29
    { 355,   17,    0,  372,    5}, // state   30
    { 377,    1,    0,    0,    0}, // state   31
    { 378,    1,    0,  379,    1}, // state   32
    { 380,   30,    0,  410,    9}, // state   33
    {   0,    0,  419,    0,    0}, // state   34
    {   0,    0,  420,    0,    0}, // state   35
    { 421,   21,    0,    0,    0}, // state   36
    {   0,    0,  442,    0,    0}, // state   37
    {   0,    0,  443,    0,    0}, // state   38
    { 444,   50,    0,    0,    0}, // state   39
    {   0,    0,  494,    0,    0}, // state   40
    {   0,    0,  495,    0,    0}, // state   41
    { 496,    2,    0,    0,    0}, // state   42
    {   0,    0,  498,    0,    0}, // state   43
    {   0,    0,  499,    0,    0}, // state   44
    {   0,    0,  500,    0,    0}, // state   45
    { 501,    2,    0,    0,    0}, // state   46
    { 503,   21,    0,    0,    0}, // state   47
    {   0,    0,  524,    0,    0}, // state   48
    { 525,   31,    0,  556,    9}, // state   49
    { 565,    5,  570,    0,    0}, // state   50
    { 571,    5,  576,    0,    0}, // state   51
    { 577,    8,  585,    0,    0}, // state   52
    { 586,    2,  588,    0,    0}, // state   53
    { 589,    5,  594,    0,    0}, // state   54
    {   0,    0,  595,    0,    0}, // state   55
    { 596,   33,    0,  629,    5}, // state   56
    {   0,    0,  634,    0,    0}, // state   57
    {   0,    0,  635,    0,    0}, // state   58
    {   0,    0,  636,    0,    0}, // state   59
    {   0,    0,  637,    0,    0}, // state   60
    { 638,    2,  640,    0,    0}, // state   61
    {   0,    0,  641,    0,    0}, // state   62
    { 642,   21,    0,    0,    0}, // state   63
    {   0,    0,  663,    0,    0}, // state   64
    { 664,   17,    0,  681,    5}, // state   65
    { 686,    1,    0,    0,    0}, // state   66
    { 687,    1,    0,    0,    0}, // state   67
    {   0,    0,  688,    0,    0}, // state   68
    { 689,   18,    0,  707,    6}, // state   69
    { 713,    1,  714,    0,    0}, // state   70
    { 715,    2,  717,    0,    0}, // state   71
    { 718,    2,  720,    0,    0}, // state   72
    { 721,    5,  726,    0,    0}, // state   73
    { 727,    2,  729,    0,    0}, // state   74
    { 730,    5,  735,    0,    0}, // state   75
    { 736,    2,  738,    0,    0}, // state   76
    { 739,    5,  744,    0,    0}, // state   77
    { 745,    2,  747,    0,    0}, // state   78
    { 748,    5,  753,    0,    0}, // state   79
    { 754,    2,    0,    0,    0}, // state   80
    { 756,    1,    0,    0,    0}, // state   81
    { 757,    2,    0,    0,    0}, // state   82
    {   0,    0,  759,    0,    0}, // state   83
    { 760,   17,    0,  777,    5}, // state   84
    { 782,   17,    0,  799,    5}, // state   85
    { 804,   50,    0,  854,    5}, // state   86
    { 859,   50,    0,  909,    5}, // state   87
    { 914,   50,    0,  964,    5}, // state   88
    { 969,   17,    0,  986,    5}, // state   89
    { 991,   17,    0, 1008,    5}, // state   90
    {1013,   17,    0, 1030,    5}, // state   91
    {1035,   17,    0, 1052,    5}, // state   92
    {1057,   17,    0, 1074,    5}, // state   93
    {1079,   17,    0, 1096,    5}, // state   94
    {1101,   17,    0, 1118,    5}, // state   95
    {1123,   17,    0, 1140,    5}, // state   96
    {1145,   17,    0, 1162,    5}, // state   97
    {1167,   17,    0, 1184,    5}, // state   98
    {1189,   17,    0, 1206,    5}, // state   99
    {1211,   17,    0, 1228,    5}, // state  100
    {   0,    0, 1233,    0,    0}, // state  101
    {   0,    0, 1234,    0,    0}, // state  102
    {1235,   17,    0, 1252,    5}, // state  103
    {1257,    1, 1258,    0,    0}, // state  104
    {1259,   50,    0, 1309,    6}, // state  105
    {1315,    1,    0,    0,    0}, // state  106
    {   0,    0, 1316,    0,    0}, // state  107
    {   0,    0, 1317,    0,    0}, // state  108
    {   0,    0, 1318,    0,    0}, // state  109
    {1319,    3,    0,    0,    0}, // state  110
    {1322,   21,    0,    0,    0}, // state  111
    {   0,    0, 1343,    0,    0}, // state  112
    {1344,    3, 1347,    0,    0}, // state  113
    {1348,   21,    0,    0,    0}, // state  114
    {1369,    2, 1371, 1372,    2}, // state  115
    {1374,    5,    0, 1379,    2}, // state  116
    {1381,    2, 1383,    0,    0}, // state  117
    {1384,   18,    0, 1402,    6}, // state  118
    {1408,   17,    0, 1425,    5}, // state  119
    {1430,   17,    0, 1447,    5}, // state  120
    {1452,   17,    0, 1469,    5}, // state  121
    {1474,    1,    0,    0,    0}, // state  122
    {1475,    1,    0,    0,    0}, // state  123
    {1476,   17,    0, 1493,    5}, // state  124
    {   0,    0, 1498,    0,    0}, // state  125
    {1499,    1,    0,    0,    0}, // state  126
    {1500,   20, 1520,    0,    0}, // state  127
    {1521,   21,    0,    0,    0}, // state  128
    {1542,    9, 1551,    0,    0}, // state  129
    {1552,    9, 1561,    0,    0}, // state  130
    {1562,    6, 1568,    0,    0}, // state  131
    {1569,    6, 1575,    0,    0}, // state  132
    {1576,    4, 1580,    0,    0}, // state  133
    {1581,    6, 1587,    0,    0}, // state  134
    {1588,    4, 1592,    0,    0}, // state  135
    {1593,   11, 1604,    0,    0}, // state  136
    {1605,   11, 1616,    0,    0}, // state  137
    {1617,   15, 1632,    0,    0}, // state  138
    {1633,   15, 1648,    0,    0}, // state  139
    {1649,   11, 1660,    0,    0}, // state  140
    {1661,   11, 1672,    0,    0}, // state  141
    {1673,   17, 1690,    0,    0}, // state  142
    {1691,   18, 1709,    0,    0}, // state  143
    {1710,    9, 1719,    0,    0}, // state  144
    {   0,    0, 1720,    0,    0}, // state  145
    {   0,    0, 1721,    0,    0}, // state  146
    {1722,   20, 1742,    0,    0}, // state  147
    {1743,    2, 1745,    0,    0}, // state  148
    {   0,    0, 1746,    0,    0}, // state  149
    {1747,   30,    0, 1777,    9}, // state  150
    {1786,   30,    0, 1816,    9}, // state  151
    {1825,   30,    0, 1855,    9}, // state  152
    {1864,   30,    0, 1894,    9}, // state  153
    {1903,    3,    0,    0,    0}, // state  154
    {   0,    0, 1906,    0,    0}, // state  155
    {1907,    1,    0,    0,    0}, // state  156
    {1908,    3,    0,    0,    0}, // state  157
    {1911,    2, 1913,    0,    0}, // state  158
    {1914,   18,    0, 1932,    5}, // state  159
    {1937,   20, 1957,    0,    0}, // state  160
    {1958,   20, 1978,    0,    0}, // state  161
    {1979,   21,    0,    0,    0}, // state  162
    {2000,    2, 2002, 2003,    2}, // state  163
    {2005,    5,    0, 2010,    2}, // state  164
    {2012,   20, 2032,    0,    0}, // state  165
    {2033,   17,    0, 2050,    5}, // state  166
    {   0,    0, 2055,    0,    0}, // state  167
    {   0,    0, 2056,    0,    0}, // state  168
    {2057,   17,    0, 2074,    5}, // state  169
    {   0,    0, 2079,    0,    0}, // state  170
    {   0,    0, 2080,    0,    0}, // state  171
    {2081,    1, 2082,    0,    0}, // state  172
    {2083,    1, 2084,    0,    0}, // state  173
    {   0,    0, 2085,    0,    0}, // state  174
    {2086,    1,    0,    0,    0}, // state  175
    {2087,    2,    0, 2089,    1}, // state  176
    {2090,    1,    0,    0,    0}, // state  177
    {2091,    1,    0,    0,    0}, // state  178
    {2092,    2, 2094,    0,    0}, // state  179
    {2095,   30,    0, 2125,    9}, // state  180
    {2134,   21, 2155,    0,    0}, // state  181
    {   0,    0, 2156,    0,    0}, // state  182
    {2157,    3,    0,    0,    0}, // state  183
    {2160,    1,    0,    0,    0}, // state  184
    {2161,    3,    0,    0,    0}, // state  185
    {2164,    3, 2167,    0,    0}, // state  186
    {2168,   21,    0,    0,    0}, // state  187
    {2189,   20, 2209,    0,    0}, // state  188
    {2210,   30,    0, 2240,    9}, // state  189
    {2249,   30,    0, 2279,    9}, // state  190
    {   0,    0, 2288, 2289,    1}, // state  191
    {   0,    0, 2290,    0,    0}, // state  192
    {   0,    0, 2291, 2292,    1}, // state  193
    {   0,    0, 2293, 2294,    1}, // state  194
    {   0,    0, 2295,    0,    0}, // state  195
    {2296,   30,    0, 2326,    9}, // state  196
    {2335,    1,    0,    0,    0}, // state  197
    {2336,    1,    0,    0,    0}, // state  198
    {2337,    1,    0,    0,    0}, // state  199
    {   0,    0, 2338,    0,    0}, // state  200
    {   0,    0, 2339,    0,    0}, // state  201
    {   0,    0, 2340,    0,    0}, // state  202
    {   0,    0, 2341,    0,    0}, // state  203
    {2342,   31,    0, 2373,    9}, // state  204
    {2382,   31,    0, 2413,    9}, // state  205
    {2422,   31,    0, 2453,    9}, // state  206
    {   0,    0, 2462,    0,    0}, // state  207
    {   0,    0, 2463, 2464,    1}, // state  208
    {   0,    0, 2465, 2466,    1}, // state  209
    {   0,    0, 2467, 2468,    1}, // state  210
    {   0,    0, 2469,    0,    0}, // state  211
    {   0,    0, 2470,    0,    0}, // state  212
    {   0,    0, 2471,    0,    0}, // state  213
    {2472,   31,    0, 2503,    9}, // state  214
    {2512,   31,    0, 2543,    9}, // state  215
    {2552,   31,    0, 2583,    9}, // state  216
    {   0,    0, 2592,    0,    0}, // state  217
    {   0,    0, 2593,    0,    0}, // state  218
    {   0,    0, 2594,    0,    0}  // state  219

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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,   35}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

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
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   16}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state    5
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  101}},

// ///////////////////////////////////////////////////////////////////////////
// state    6
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   47}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state    7
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,   48}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,   49}},

// ///////////////////////////////////////////////////////////////////////////
// state    8
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   50}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state    9
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   51}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   10
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   52}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   11
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   53}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   54}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   12
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   55}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   56}},

// ///////////////////////////////////////////////////////////////////////////
// state   13
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   51}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   57}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   58}},
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
    {                Token::BOOLEAN, {           TA_REDUCE_USING_RULE,   51}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,   51}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,   51}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,   51}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,   51}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   51}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   51}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   51}},

// ///////////////////////////////////////////////////////////////////////////
// state   14
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   54}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   59}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   60}},
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
    {                Token::BOOLEAN, {           TA_REDUCE_USING_RULE,   54}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,   54}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,   54}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,   54}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,   54}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   54}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   54}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   54}},

// ///////////////////////////////////////////////////////////////////////////
// state   15
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   41}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   41}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   41}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   41}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   63}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   16
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   65}},

// ///////////////////////////////////////////////////////////////////////////
// state   17
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   67}},

// ///////////////////////////////////////////////////////////////////////////
// state   18
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  105}},

// ///////////////////////////////////////////////////////////////////////////
// state   19
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  104}},

// ///////////////////////////////////////////////////////////////////////////
// state   20
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state   21
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   69}},

// ///////////////////////////////////////////////////////////////////////////
// state   22
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   70}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   71}},

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
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   59}},

// ///////////////////////////////////////////////////////////////////////////
// state   27
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   72}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   73}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   28
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   74}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   75}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   29
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   76}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   77}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   30
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   78}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   79}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   31
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   32
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    // nonterminal transitions
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   81}},

// ///////////////////////////////////////////////////////////////////////////
// state   33
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,   82}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   34
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   18}},

// ///////////////////////////////////////////////////////////////////////////
// state   35
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   15}},

// ///////////////////////////////////////////////////////////////////////////
// state   36
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state   37
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   17}},

// ///////////////////////////////////////////////////////////////////////////
// state   38
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   60}},

// ///////////////////////////////////////////////////////////////////////////
// state   39
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,  113}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,  113}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,  113}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,  113}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,  113}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,  113}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,  113}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,  113}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,  113}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,  113}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,  113}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,  113}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,  113}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,  113}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,  113}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,  113}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,  113}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,  113}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,  113}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,  113}},
    {        Token::FUNC_IDENTIFIER, {           TA_REDUCE_USING_RULE,  113}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,  113}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,  113}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,  113}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,  113}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,  113}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,  113}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,  113}},
    {                Token::BOOLEAN, {           TA_REDUCE_USING_RULE,  113}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,  113}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,  113}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,  113}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,  113}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,  113}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,  113}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,  113}},

// ///////////////////////////////////////////////////////////////////////////
// state   40
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   61}},

// ///////////////////////////////////////////////////////////////////////////
// state   41
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   55}},

// ///////////////////////////////////////////////////////////////////////////
// state   42
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  107}},

// ///////////////////////////////////////////////////////////////////////////
// state   43
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   93}},

// ///////////////////////////////////////////////////////////////////////////
// state   44
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state   45
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},

// ///////////////////////////////////////////////////////////////////////////
// state   46
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state   47
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  108}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state   48
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   20}},

// ///////////////////////////////////////////////////////////////////////////
// state   49
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  109}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,   35}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   50
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   84}},

// ///////////////////////////////////////////////////////////////////////////
// state   51
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   85}},

// ///////////////////////////////////////////////////////////////////////////
// state   52
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   66}},

// ///////////////////////////////////////////////////////////////////////////
// state   53
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   86}},

// ///////////////////////////////////////////////////////////////////////////
// state   54
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   88}},

// ///////////////////////////////////////////////////////////////////////////
// state   55
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   26}},

// ///////////////////////////////////////////////////////////////////////////
// state   56
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   25}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  110}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   25}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   25}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   25}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  111}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   57
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   50}},

// ///////////////////////////////////////////////////////////////////////////
// state   58
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   49}},

// ///////////////////////////////////////////////////////////////////////////
// state   59
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   53}},

// ///////////////////////////////////////////////////////////////////////////
// state   60
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   52}},

// ///////////////////////////////////////////////////////////////////////////
// state   61
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   62
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state   63
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  112}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state   64
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   65
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  113}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  114}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   66
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  115}},

// ///////////////////////////////////////////////////////////////////////////
// state   67
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  116}},

// ///////////////////////////////////////////////////////////////////////////
// state   68
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   48}},

// ///////////////////////////////////////////////////////////////////////////
// state   69
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  117}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  118}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   70
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  119}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  114}},

// ///////////////////////////////////////////////////////////////////////////
// state   71
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  120}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,  121}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  118}},

// ///////////////////////////////////////////////////////////////////////////
// state   72
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   92}},

// ///////////////////////////////////////////////////////////////////////////
// state   73
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   91}},

// ///////////////////////////////////////////////////////////////////////////
// state   74
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state   75
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   95}},

// ///////////////////////////////////////////////////////////////////////////
// state   76
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   87}},

// ///////////////////////////////////////////////////////////////////////////
// state   77
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   89}},

// ///////////////////////////////////////////////////////////////////////////
// state   78
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  100}},

// ///////////////////////////////////////////////////////////////////////////
// state   79
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   99}},

// ///////////////////////////////////////////////////////////////////////////
// state   80
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  122}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,  123}},

// ///////////////////////////////////////////////////////////////////////////
// state   81
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  124}},

// ///////////////////////////////////////////////////////////////////////////
// state   82
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  125}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,  126}},

// ///////////////////////////////////////////////////////////////////////////
// state   83
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state   84
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  127}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   85
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  128}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   86
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   64}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   64}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   64}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   64}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  129}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   87
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   63}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   63}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   63}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   63}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  130}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   88
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   65}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   65}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   65}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,   65}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   65}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   65}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,   65}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,   65}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   65}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,   65}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,   65}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,   65}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,   65}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,   65}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,   65}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,   65}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,   65}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,   65}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,   65}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,   65}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,   65}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   65}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   65}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   65}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   65}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   65}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   65}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   65}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   65}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   65}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   65}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   65}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   65}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  131}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   89
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  132}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   90
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  133}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   91
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  134}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   92
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  135}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   93
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  136}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   94
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  137}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   95
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  138}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   96
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  139}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   97
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  140}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   98
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  141}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   99
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  142}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  100
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  143}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  101
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   94}},

// ///////////////////////////////////////////////////////////////////////////
// state  102
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   97}},

// ///////////////////////////////////////////////////////////////////////////
// state  103
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  144}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  104
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  145}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  112}},

// ///////////////////////////////////////////////////////////////////////////
// state  105
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,  110}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,  110}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  146}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,  110}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,  110}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,  110}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,  110}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,  110}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,  110}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,  110}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,  110}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,  110}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,  110}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,  110}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,  110}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,  110}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,  110}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,  110}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,  110}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,  110}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,  110}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,  110}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,  110}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,  110}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,  110}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,  110}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,  110}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,  110}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,  110}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,  110}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,  110}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,  110}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,  110}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  147}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {           Token::param_list__, {                  TA_PUSH_STATE,  148}},

// ///////////////////////////////////////////////////////////////////////////
// state  106
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  149}},

// ///////////////////////////////////////////////////////////////////////////
// state  107
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   21}},

// ///////////////////////////////////////////////////////////////////////////
// state  108
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   83}},

// ///////////////////////////////////////////////////////////////////////////
// state  109
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   19}},

// ///////////////////////////////////////////////////////////////////////////
// state  110
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  150}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state  111
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  151}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state  112
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  113
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  152}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state  114
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  153}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state  115
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  154}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  155}},

// ///////////////////////////////////////////////////////////////////////////
// state  116
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  156}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   10}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   10}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  157}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  155}},

// ///////////////////////////////////////////////////////////////////////////
// state  117
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state  118
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  158}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  159}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  119
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  160}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  120
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  161}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  121
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  162}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  122
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  163}},

// ///////////////////////////////////////////////////////////////////////////
// state  123
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  164}},

// ///////////////////////////////////////////////////////////////////////////
// state  124
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  165}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  125
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   30}},

// ///////////////////////////////////////////////////////////////////////////
// state  126
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  166}},

// ///////////////////////////////////////////////////////////////////////////
// state  127
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   72}},

// ///////////////////////////////////////////////////////////////////////////
// state  128
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  167}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state  129
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   67}},

// ///////////////////////////////////////////////////////////////////////////
// state  130
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   62}},

// ///////////////////////////////////////////////////////////////////////////
// state  131
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   68}},

// ///////////////////////////////////////////////////////////////////////////
// state  132
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   69}},

// ///////////////////////////////////////////////////////////////////////////
// state  133
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   73}},

// ///////////////////////////////////////////////////////////////////////////
// state  134
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   70}},

// ///////////////////////////////////////////////////////////////////////////
// state  135
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   71}},

// ///////////////////////////////////////////////////////////////////////////
// state  136
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state  137
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   78}},

// ///////////////////////////////////////////////////////////////////////////
// state  138
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   76}},

// ///////////////////////////////////////////////////////////////////////////
// state  139
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   77}},

// ///////////////////////////////////////////////////////////////////////////
// state  140
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   81}},

// ///////////////////////////////////////////////////////////////////////////
// state  141
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state  142
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   75}},

// ///////////////////////////////////////////////////////////////////////////
// state  143
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   74}},

// ///////////////////////////////////////////////////////////////////////////
// state  144
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   82}},

// ///////////////////////////////////////////////////////////////////////////
// state  145
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  111}},

// ///////////////////////////////////////////////////////////////////////////
// state  146
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  107}},

// ///////////////////////////////////////////////////////////////////////////
// state  147
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  120}},

// ///////////////////////////////////////////////////////////////////////////
// state  148
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  168}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  169}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  109}},

// ///////////////////////////////////////////////////////////////////////////
// state  149
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   23}},

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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,  170}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,  171}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,  172}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  153
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,  173}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  154
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  174}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  175}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  176}},

// ///////////////////////////////////////////////////////////////////////////
// state  155
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},

// ///////////////////////////////////////////////////////////////////////////
// state  156
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  177}},

// ///////////////////////////////////////////////////////////////////////////
// state  157
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  174}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  178}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  176}},

// ///////////////////////////////////////////////////////////////////////////
// state  158
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state  159
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  179}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  180}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  181}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  160
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  115}},

// ///////////////////////////////////////////////////////////////////////////
// state  161
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  119}},

// ///////////////////////////////////////////////////////////////////////////
// state  162
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  182}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state  163
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  183}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  155}},

// ///////////////////////////////////////////////////////////////////////////
// state  164
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  184}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   10}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   10}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  185}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  155}},

// ///////////////////////////////////////////////////////////////////////////
// state  165
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  116}},

// ///////////////////////////////////////////////////////////////////////////
// state  166
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  186}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  187}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  167
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   90}},

// ///////////////////////////////////////////////////////////////////////////
// state  168
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  108}},

// ///////////////////////////////////////////////////////////////////////////
// state  169
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   46}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  188}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  170
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   27}},

// ///////////////////////////////////////////////////////////////////////////
// state  171
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   24}},

// ///////////////////////////////////////////////////////////////////////////
// state  172
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  189}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   35}},

// ///////////////////////////////////////////////////////////////////////////
// state  173
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  190}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   33}},

// ///////////////////////////////////////////////////////////////////////////
// state  174
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},

// ///////////////////////////////////////////////////////////////////////////
// state  175
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  191}},

// ///////////////////////////////////////////////////////////////////////////
// state  176
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,  192}},

// ///////////////////////////////////////////////////////////////////////////
// state  177
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  193}},

// ///////////////////////////////////////////////////////////////////////////
// state  178
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  194}},

// ///////////////////////////////////////////////////////////////////////////
// state  179
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   46}},

// ///////////////////////////////////////////////////////////////////////////
// state  180
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,  195}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  181
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  196}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   47}},

// ///////////////////////////////////////////////////////////////////////////
// state  182
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  117}},

// ///////////////////////////////////////////////////////////////////////////
// state  183
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  174}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  197}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  176}},

// ///////////////////////////////////////////////////////////////////////////
// state  184
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  198}},

// ///////////////////////////////////////////////////////////////////////////
// state  185
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  174}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  199}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  176}},

// ///////////////////////////////////////////////////////////////////////////
// state  186
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  200}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   43}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   31}},

// ///////////////////////////////////////////////////////////////////////////
// state  187
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  201}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state  188
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  121}},

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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,  202}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  190
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,  203}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  191
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  204}},

// ///////////////////////////////////////////////////////////////////////////
// state  192
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   12}},

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
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  206}},

// ///////////////////////////////////////////////////////////////////////////
// state  195
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  196
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,  207}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

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
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  210}},

// ///////////////////////////////////////////////////////////////////////////
// state  200
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   29}},

// ///////////////////////////////////////////////////////////////////////////
// state  201
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   28}},

// ///////////////////////////////////////////////////////////////////////////
// state  202
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   34}},

// ///////////////////////////////////////////////////////////////////////////
// state  203
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   32}},

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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,   35}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,   35}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  206
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  213}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,   35}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  207
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   43}},

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
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  216}},

// ///////////////////////////////////////////////////////////////////////////
// state  211
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    5}},

// ///////////////////////////////////////////////////////////////////////////
// state  212
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    3}},

// ///////////////////////////////////////////////////////////////////////////
// state  213
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    2}},

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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,   35}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,   35}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  216
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  219}},
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
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   31}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    {                     Token::DO, {        TA_SHIFT_AND_PUSH_STATE,   33}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   34}},
    {            Token::statement__, {                  TA_PUSH_STATE,   35}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   36}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   37}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   38}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   39}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   40}},
    {                 Token::call__, {                  TA_PUSH_STATE,   41}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  217
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    6}},

// ///////////////////////////////////////////////////////////////////////////
// state  218
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    4}},

// ///////////////////////////////////////////////////////////////////////////
// state  219
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

#line 6344 "SteelParser.cpp"

