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

#line 131 "steel.trison"

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

#line 144 "steel.trison"

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

#line 154 "steel.trison"

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

#line 166 "steel.trison"

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

// rule 5: func_definition <- FUNCTION %error '(' ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0005 ()
{
    assert(5 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);

#line 179 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									NULL,
									stmts,
									false);
				
#line 595 "SteelParser.cpp"
}

// rule 6: func_definition <- FINAL FUNCTION %error '(' ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0006 ()
{
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 192 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									NULL,
									stmts,
									true);
				
#line 616 "SteelParser.cpp"
}

// rule 7: func_definition <- FUNCTION %error '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0007 ()
{
    assert(3 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 206 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									params,
									stmts,
									false);
				
#line 639 "SteelParser.cpp"
}

// rule 8: func_definition <- FINAL FUNCTION %error '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0008 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 219 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									params,
									stmts,
									true);
				
#line 662 "SteelParser.cpp"
}

// rule 9: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0009 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(5 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);

#line 233 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									NULL,
									stmts,
									false);
				
#line 682 "SteelParser.cpp"
}

// rule 10: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0010 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 243 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									true);
				
#line 704 "SteelParser.cpp"
}

// rule 11: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0011 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 253 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									NULL,
									stmts,
									true);
				
#line 724 "SteelParser.cpp"
}

// rule 12: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' ')' '{' statement_list:stmts %error    
AstBase* SteelParser::ReductionRuleHandler0012 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(5 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);

#line 263 "steel.trison"

					addError(GET_LINE(),"expected }");
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									NULL,
									stmts,
									false);
				
#line 745 "SteelParser.cpp"
}

// rule 13: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' ')' '{' statement_list:stmts %error    
AstBase* SteelParser::ReductionRuleHandler0013 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 274 "steel.trison"

					addError(GET_LINE(),"expected }");
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									NULL,
									stmts,
									true);
				
#line 766 "SteelParser.cpp"
}

// rule 14: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' param_definition:params ')' '{' statement_list:stmts %error    
AstBase* SteelParser::ReductionRuleHandler0014 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 285 "steel.trison"

					addError(GET_LINE(),"expected }");
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									false);
				
#line 789 "SteelParser.cpp"
}

// rule 15: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' param_definition:params ')' '{' statement_list:stmts %error    
AstBase* SteelParser::ReductionRuleHandler0015 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 296 "steel.trison"

					addError(GET_LINE(),"expected }");
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									true);
				
#line 812 "SteelParser.cpp"
}

// rule 16: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' ')' %error    
AstBase* SteelParser::ReductionRuleHandler0016 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 307 "steel.trison"

					addError(GET_LINE(),"expected function body.");
					return new AstFunctionDefinition(id->GetLine(),	
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									NULL,
									new AstStatementList(GET_LINE(),GET_SCRIPT()),
									false);
				
#line 831 "SteelParser.cpp"
}

// rule 17: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' ')' %error    
AstBase* SteelParser::ReductionRuleHandler0017 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 319 "steel.trison"

					addError(GET_LINE(),"expected function body.");
					return new AstFunctionDefinition(id->GetLine(),	
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									NULL,
									new AstStatementList(GET_LINE(), GET_SCRIPT()),
									true);
				
#line 850 "SteelParser.cpp"
}

// rule 18: param_id <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0018 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 334 "steel.trison"
 return id; 
#line 861 "SteelParser.cpp"
}

// rule 19: param_id <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0019 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 336 "steel.trison"
 return id; 
#line 872 "SteelParser.cpp"
}

// rule 20: param_definition <- vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0020 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 342 "steel.trison"

				AstParamDefinitionList * pList = new AstParamDefinitionList(decl->GetLine(),decl->GetScript());
				pList->add(static_cast<AstDeclaration*>(decl));
				return pList;		
			
#line 887 "SteelParser.cpp"
}

// rule 21: param_definition <- param_definition:list ',' vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0021 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamDefinitionList* list = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 349 "steel.trison"

				list->add(static_cast<AstDeclaration*>(decl));
				return list;
			
#line 903 "SteelParser.cpp"
}

// rule 22: statement_list <-     
AstBase* SteelParser::ReductionRuleHandler0022 ()
{

#line 357 "steel.trison"

				AstStatementList *pList = 
					new AstStatementList(m_scanner->getCurrentLine(),
										m_scanner->getScriptName());
				return pList;
			
#line 917 "SteelParser.cpp"
}

// rule 23: statement_list <- statement_list:list statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0023 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 365 "steel.trison"

					list->add( stmt ); 
					return list;
				
#line 933 "SteelParser.cpp"
}

// rule 24: statement <- exp_statement:exp    
AstBase* SteelParser::ReductionRuleHandler0024 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 373 "steel.trison"
 return new AstExpressionStatement(exp->GetLine(),exp->GetScript(), exp); 
#line 944 "SteelParser.cpp"
}

// rule 25: statement <- func_definition:func    
AstBase* SteelParser::ReductionRuleHandler0025 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFunctionDefinition* func = static_cast< AstFunctionDefinition* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 375 "steel.trison"
 return func; 
#line 955 "SteelParser.cpp"
}

// rule 26: statement <- '{' statement_list:list '}'    
AstBase* SteelParser::ReductionRuleHandler0026 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 377 "steel.trison"
 return list; 
#line 966 "SteelParser.cpp"
}

// rule 27: statement <- '{' '}'    
AstBase* SteelParser::ReductionRuleHandler0027 ()
{

#line 379 "steel.trison"

			 return new AstStatement(GET_LINE(),GET_SCRIPT());
			
#line 977 "SteelParser.cpp"
}

// rule 28: statement <- vardecl:vardecl ';'    
AstBase* SteelParser::ReductionRuleHandler0028 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* vardecl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 383 "steel.trison"
 return vardecl; 
#line 988 "SteelParser.cpp"
}

// rule 29: statement <- WHILE '(' exp:exp ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0029 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 385 "steel.trison"
 return new AstWhileStatement(exp->GetLine(), exp->GetScript(),exp,stmt); 
#line 1001 "SteelParser.cpp"
}

// rule 30: statement <- IF '(' exp:exp ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0030 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 387 "steel.trison"
 return new AstIfStatement(exp->GetLine(), exp->GetScript(),exp,stmt,elses);
#line 1016 "SteelParser.cpp"
}

// rule 31: statement <- IF '(' exp:exp ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0031 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 389 "steel.trison"
 return new AstIfStatement(exp->GetLine(),exp->GetScript(),exp,stmt); 
#line 1029 "SteelParser.cpp"
}

// rule 32: statement <- RETURN exp:exp ';'    
AstBase* SteelParser::ReductionRuleHandler0032 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 391 "steel.trison"
 return new AstReturnStatement(exp->GetLine(),exp->GetScript(),exp);
#line 1040 "SteelParser.cpp"
}

// rule 33: statement <- RETURN ';'    
AstBase* SteelParser::ReductionRuleHandler0033 ()
{

#line 394 "steel.trison"

				return new AstReturnStatement(GET_LINE(),GET_SCRIPT());
			
#line 1051 "SteelParser.cpp"
}

// rule 34: statement <- FOR '(' exp_statement:start exp_statement:condition ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0034 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(5 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);

#line 399 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition, 
						new AstExpression (start->GetLine(),start->GetScript()), stmt); 
			
#line 1069 "SteelParser.cpp"
}

// rule 35: statement <- FOR '(' exp_statement:start exp_statement:condition exp:iteration ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0035 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* iteration = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 405 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition,iteration,stmt);
			
#line 1088 "SteelParser.cpp"
}

// rule 36: statement <- BREAK ';'    
AstBase* SteelParser::ReductionRuleHandler0036 ()
{

#line 410 "steel.trison"

				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1099 "SteelParser.cpp"
}

// rule 37: statement <- CONTINUE ';'    
AstBase* SteelParser::ReductionRuleHandler0037 ()
{

#line 415 "steel.trison"

				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1110 "SteelParser.cpp"
}

// rule 38: exp <- call:call    
AstBase* SteelParser::ReductionRuleHandler0038 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstCallExpression* call = static_cast< AstCallExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 423 "steel.trison"
 return call; 
#line 1121 "SteelParser.cpp"
}

// rule 39: exp <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0039 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 425 "steel.trison"
 return i;
#line 1132 "SteelParser.cpp"
}

// rule 40: exp <- FLOAT:f    
AstBase* SteelParser::ReductionRuleHandler0040 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* f = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 427 "steel.trison"
 return f; 
#line 1143 "SteelParser.cpp"
}

// rule 41: exp <- STRING:s    
AstBase* SteelParser::ReductionRuleHandler0041 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* s = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 429 "steel.trison"
 return s; 
#line 1154 "SteelParser.cpp"
}

// rule 42: exp <- var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0042 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 431 "steel.trison"
 return id; 
#line 1165 "SteelParser.cpp"
}

// rule 43: exp <- array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0043 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 433 "steel.trison"
 return id; 
#line 1176 "SteelParser.cpp"
}

// rule 44: exp <- exp:a '+' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0044 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 435 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,b); 
#line 1189 "SteelParser.cpp"
}

// rule 45: exp <- exp:a '-' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0045 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 437 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,b); 
#line 1202 "SteelParser.cpp"
}

// rule 46: exp <- exp:a '*' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0046 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 439 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,b); 
#line 1215 "SteelParser.cpp"
}

// rule 47: exp <- exp:a '/' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0047 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 441 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::DIV,a,b); 
#line 1228 "SteelParser.cpp"
}

// rule 48: exp <- exp:a '%' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0048 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 443 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MOD,a,b); 
#line 1241 "SteelParser.cpp"
}

// rule 49: exp <- exp:a D exp:b    %left %prec POW
AstBase* SteelParser::ReductionRuleHandler0049 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 445 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::D,a,b); 
#line 1254 "SteelParser.cpp"
}

// rule 50: exp <- exp:lvalue '=' exp:exp    %right %prec ASSIGNMENT
AstBase* SteelParser::ReductionRuleHandler0050 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 447 "steel.trison"
 return new AstVarAssignmentExpression(lvalue->GetLine(),lvalue->GetScript(),lvalue,exp); 
#line 1267 "SteelParser.cpp"
}

// rule 51: exp <- exp:a '^' exp:b    %right %prec POW
AstBase* SteelParser::ReductionRuleHandler0051 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 449 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::POW,a,b); 
#line 1280 "SteelParser.cpp"
}

// rule 52: exp <- exp:a OR exp:b    %left %prec OR
AstBase* SteelParser::ReductionRuleHandler0052 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 451 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::OR,a,b); 
#line 1293 "SteelParser.cpp"
}

// rule 53: exp <- exp:a AND exp:b    %left %prec AND
AstBase* SteelParser::ReductionRuleHandler0053 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 453 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::AND,a,b); 
#line 1306 "SteelParser.cpp"
}

// rule 54: exp <- exp:a EQ exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0054 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 455 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::EQ,a,b); 
#line 1319 "SteelParser.cpp"
}

// rule 55: exp <- exp:a NE exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0055 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 457 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::NE,a,b); 
#line 1332 "SteelParser.cpp"
}

// rule 56: exp <- exp:a LT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0056 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 459 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LT,a,b); 
#line 1345 "SteelParser.cpp"
}

// rule 57: exp <- exp:a GT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0057 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 461 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GT,a,b); 
#line 1358 "SteelParser.cpp"
}

// rule 58: exp <- exp:a LTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0058 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 463 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LTE,a,b); 
#line 1371 "SteelParser.cpp"
}

// rule 59: exp <- exp:a GTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0059 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 465 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GTE,a,b); 
#line 1384 "SteelParser.cpp"
}

// rule 60: exp <- exp:a CAT exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0060 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 467 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::CAT,a,b); 
#line 1397 "SteelParser.cpp"
}

// rule 61: exp <- '(' exp:exp ')'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0061 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 469 "steel.trison"
 return exp; 
#line 1408 "SteelParser.cpp"
}

// rule 62: exp <- '-' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0062 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 471 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::MINUS,exp); 
#line 1419 "SteelParser.cpp"
}

// rule 63: exp <- '+' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0063 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 473 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::PLUS,exp); 
#line 1430 "SteelParser.cpp"
}

// rule 64: exp <- NOT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0064 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 475 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::NOT,exp); 
#line 1441 "SteelParser.cpp"
}

// rule 65: exp <- CAT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0065 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 477 "steel.trison"
 return new AstUnaryOp(exp->GetLine(),
exp->GetScript(), AstUnaryOp::CAT,exp); 
#line 1453 "SteelParser.cpp"
}

// rule 66: exp <- exp:lvalue '[' exp:index ']'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0066 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* index = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 480 "steel.trison"
 return new AstArrayElement(lvalue->GetLine(),lvalue->GetScript(),lvalue,index); 
#line 1466 "SteelParser.cpp"
}

// rule 67: exp <- INCREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0067 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 482 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::PRE);
#line 1477 "SteelParser.cpp"
}

// rule 68: exp <- exp:lvalue INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0068 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 484 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::POST);
#line 1488 "SteelParser.cpp"
}

// rule 69: exp <- DECREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0069 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 486 "steel.trison"
 return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::PRE);
#line 1499 "SteelParser.cpp"
}

// rule 70: exp <- exp:lvalue DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0070 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 488 "steel.trison"
 return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::POST);
#line 1510 "SteelParser.cpp"
}

// rule 71: exp <- POP exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0071 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 490 "steel.trison"
 return new AstPop(lvalue->GetLine(),lvalue->GetScript(),lvalue); 
#line 1521 "SteelParser.cpp"
}

// rule 72: exp_statement <- ';'    
AstBase* SteelParser::ReductionRuleHandler0072 ()
{

#line 495 "steel.trison"

			return new AstExpression(GET_LINE(),GET_SCRIPT()); 
		
#line 1532 "SteelParser.cpp"
}

// rule 73: exp_statement <- exp:exp ';'    
AstBase* SteelParser::ReductionRuleHandler0073 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 499 "steel.trison"
 return exp; 
#line 1543 "SteelParser.cpp"
}

// rule 74: int_literal <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0074 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 504 "steel.trison"
 return i; 
#line 1554 "SteelParser.cpp"
}

// rule 75: var_identifier <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0075 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 509 "steel.trison"
 return id; 
#line 1565 "SteelParser.cpp"
}

// rule 76: func_identifier <- FUNC_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0076 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 514 "steel.trison"
 return id; 
#line 1576 "SteelParser.cpp"
}

// rule 77: array_identifier <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0077 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 519 "steel.trison"
 return id; 
#line 1587 "SteelParser.cpp"
}

// rule 78: call <- func_identifier:id '(' ')'    
AstBase* SteelParser::ReductionRuleHandler0078 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 525 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id);
#line 1598 "SteelParser.cpp"
}

// rule 79: call <- func_identifier:id '(' param_list:params ')'    
AstBase* SteelParser::ReductionRuleHandler0079 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 527 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id,params);
#line 1611 "SteelParser.cpp"
}

// rule 80: vardecl <- VAR var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0080 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 532 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id);
#line 1622 "SteelParser.cpp"
}

// rule 81: vardecl <- VAR var_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0081 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 534 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id,exp); 
#line 1635 "SteelParser.cpp"
}

// rule 82: vardecl <- VAR array_identifier:id '[' exp:i ']'    
AstBase* SteelParser::ReductionRuleHandler0082 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* i = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 536 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id,i); 
#line 1648 "SteelParser.cpp"
}

// rule 83: vardecl <- VAR array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0083 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 538 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id); 
#line 1659 "SteelParser.cpp"
}

// rule 84: vardecl <- VAR array_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0084 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 540 "steel.trison"

							AstArrayDeclaration *pDecl =  new AstArrayDeclaration(id->GetLine(),id->GetScript(),id);
							pDecl->assign(exp);
							return pDecl;
						 
#line 1676 "SteelParser.cpp"
}

// rule 85: param_list <- exp:exp    
AstBase* SteelParser::ReductionRuleHandler0085 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 549 "steel.trison"
 AstParamList * pList = new AstParamList ( exp->GetLine(), exp->GetScript() );
		  pList->add(exp);
		  return pList;
		
#line 1690 "SteelParser.cpp"
}

// rule 86: param_list <- param_list:list ',' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0086 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamList* list = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 554 "steel.trison"
 list->add(exp); return list;
#line 1703 "SteelParser.cpp"
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
    {      Token::func_definition__,  7, &SteelParser::ReductionRuleHandler0005, "rule 5: func_definition <- FUNCTION %error '(' ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0006, "rule 6: func_definition <- FINAL FUNCTION %error '(' ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0007, "rule 7: func_definition <- FUNCTION %error '(' param_definition ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  9, &SteelParser::ReductionRuleHandler0008, "rule 8: func_definition <- FINAL FUNCTION %error '(' param_definition ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  7, &SteelParser::ReductionRuleHandler0009, "rule 9: func_definition <- FUNCTION FUNC_IDENTIFIER '(' ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  9, &SteelParser::ReductionRuleHandler0010, "rule 10: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER '(' param_definition ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0011, "rule 11: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER '(' ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  7, &SteelParser::ReductionRuleHandler0012, "rule 12: func_definition <- FUNCTION FUNC_IDENTIFIER '(' ')' '{' statement_list %error    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0013, "rule 13: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER '(' ')' '{' statement_list %error    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0014, "rule 14: func_definition <- FUNCTION FUNC_IDENTIFIER '(' param_definition ')' '{' statement_list %error    "},
    {      Token::func_definition__,  9, &SteelParser::ReductionRuleHandler0015, "rule 15: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER '(' param_definition ')' '{' statement_list %error    "},
    {      Token::func_definition__,  5, &SteelParser::ReductionRuleHandler0016, "rule 16: func_definition <- FUNCTION FUNC_IDENTIFIER '(' ')' %error    "},
    {      Token::func_definition__,  6, &SteelParser::ReductionRuleHandler0017, "rule 17: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER '(' ')' %error    "},
    {             Token::param_id__,  1, &SteelParser::ReductionRuleHandler0018, "rule 18: param_id <- VAR_IDENTIFIER    "},
    {             Token::param_id__,  1, &SteelParser::ReductionRuleHandler0019, "rule 19: param_id <- ARRAY_IDENTIFIER    "},
    {     Token::param_definition__,  1, &SteelParser::ReductionRuleHandler0020, "rule 20: param_definition <- vardecl    "},
    {     Token::param_definition__,  3, &SteelParser::ReductionRuleHandler0021, "rule 21: param_definition <- param_definition ',' vardecl    "},
    {       Token::statement_list__,  0, &SteelParser::ReductionRuleHandler0022, "rule 22: statement_list <-     "},
    {       Token::statement_list__,  2, &SteelParser::ReductionRuleHandler0023, "rule 23: statement_list <- statement_list statement    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0024, "rule 24: statement <- exp_statement    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0025, "rule 25: statement <- func_definition    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0026, "rule 26: statement <- '{' statement_list '}'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0027, "rule 27: statement <- '{' '}'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0028, "rule 28: statement <- vardecl ';'    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0029, "rule 29: statement <- WHILE '(' exp ')' statement    "},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0030, "rule 30: statement <- IF '(' exp ')' statement ELSE statement     %prec ELSE"},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0031, "rule 31: statement <- IF '(' exp ')' statement     %prec NON_ELSE"},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0032, "rule 32: statement <- RETURN exp ';'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0033, "rule 33: statement <- RETURN ';'    "},
    {            Token::statement__,  6, &SteelParser::ReductionRuleHandler0034, "rule 34: statement <- FOR '(' exp_statement exp_statement ')' statement    "},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0035, "rule 35: statement <- FOR '(' exp_statement exp_statement exp ')' statement    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0036, "rule 36: statement <- BREAK ';'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0037, "rule 37: statement <- CONTINUE ';'    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0038, "rule 38: exp <- call    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0039, "rule 39: exp <- INT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0040, "rule 40: exp <- FLOAT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0041, "rule 41: exp <- STRING    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0042, "rule 42: exp <- var_identifier    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0043, "rule 43: exp <- array_identifier    "},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0044, "rule 44: exp <- exp '+' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0045, "rule 45: exp <- exp '-' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0046, "rule 46: exp <- exp '*' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0047, "rule 47: exp <- exp '/' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0048, "rule 48: exp <- exp '%' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0049, "rule 49: exp <- exp D exp    %left %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0050, "rule 50: exp <- exp '=' exp    %right %prec ASSIGNMENT"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0051, "rule 51: exp <- exp '^' exp    %right %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0052, "rule 52: exp <- exp OR exp    %left %prec OR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0053, "rule 53: exp <- exp AND exp    %left %prec AND"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0054, "rule 54: exp <- exp EQ exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0055, "rule 55: exp <- exp NE exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0056, "rule 56: exp <- exp LT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0057, "rule 57: exp <- exp GT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0058, "rule 58: exp <- exp LTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0059, "rule 59: exp <- exp GTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0060, "rule 60: exp <- exp CAT exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0061, "rule 61: exp <- '(' exp ')'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0062, "rule 62: exp <- '-' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0063, "rule 63: exp <- '+' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0064, "rule 64: exp <- NOT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0065, "rule 65: exp <- CAT exp     %prec UNARY"},
    {                  Token::exp__,  4, &SteelParser::ReductionRuleHandler0066, "rule 66: exp <- exp '[' exp ']'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0067, "rule 67: exp <- INCREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0068, "rule 68: exp <- exp INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0069, "rule 69: exp <- DECREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0070, "rule 70: exp <- exp DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0071, "rule 71: exp <- POP exp     %prec UNARY"},
    {        Token::exp_statement__,  1, &SteelParser::ReductionRuleHandler0072, "rule 72: exp_statement <- ';'    "},
    {        Token::exp_statement__,  2, &SteelParser::ReductionRuleHandler0073, "rule 73: exp_statement <- exp ';'    "},
    {          Token::int_literal__,  1, &SteelParser::ReductionRuleHandler0074, "rule 74: int_literal <- INT    "},
    {       Token::var_identifier__,  1, &SteelParser::ReductionRuleHandler0075, "rule 75: var_identifier <- VAR_IDENTIFIER    "},
    {      Token::func_identifier__,  1, &SteelParser::ReductionRuleHandler0076, "rule 76: func_identifier <- FUNC_IDENTIFIER    "},
    {     Token::array_identifier__,  1, &SteelParser::ReductionRuleHandler0077, "rule 77: array_identifier <- ARRAY_IDENTIFIER    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0078, "rule 78: call <- func_identifier '(' ')'    "},
    {                 Token::call__,  4, &SteelParser::ReductionRuleHandler0079, "rule 79: call <- func_identifier '(' param_list ')'    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0080, "rule 80: vardecl <- VAR var_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0081, "rule 81: vardecl <- VAR var_identifier '=' exp    "},
    {              Token::vardecl__,  5, &SteelParser::ReductionRuleHandler0082, "rule 82: vardecl <- VAR array_identifier '[' exp ']'    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0083, "rule 83: vardecl <- VAR array_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0084, "rule 84: vardecl <- VAR array_identifier '=' exp    "},
    {           Token::param_list__,  1, &SteelParser::ReductionRuleHandler0085, "rule 85: param_list <- exp    "},
    {           Token::param_list__,  3, &SteelParser::ReductionRuleHandler0086, "rule 86: param_list <- param_list ',' exp    "},

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
    {   5,   25,   30,   31,    9}, // state    2
    {   0,    0,   40,    0,    0}, // state    3
    {   0,    0,   41,    0,    0}, // state    4
    {  42,   14,    0,   56,    5}, // state    5
    {  61,    1,   62,   63,    1}, // state    6
    {  64,   14,    0,   78,    5}, // state    7
    {  83,   14,    0,   97,    5}, // state    8
    { 102,   14,    0,  116,    5}, // state    9
    { 121,    1,    0,    0,    0}, // state   10
    { 122,    1,    0,    0,    0}, // state   11
    { 123,    1,    0,    0,    0}, // state   12
    { 124,   15,    0,  139,    5}, // state   13
    { 144,    1,    0,    0,    0}, // state   14
    { 145,    2,    0,    0,    0}, // state   15
    {   0,    0,  147,    0,    0}, // state   16
    {   0,    0,  148,    0,    0}, // state   17
    {   0,    0,  149,    0,    0}, // state   18
    { 150,    1,    0,    0,    0}, // state   19
    { 151,    2,    0,  153,    2}, // state   20
    {   0,    0,  155,    0,    0}, // state   21
    {   0,    0,  156,    0,    0}, // state   22
    {   0,    0,  157,    0,    0}, // state   23
    { 158,   14,    0,  172,    5}, // state   24
    { 177,   14,    0,  191,    5}, // state   25
    { 196,   14,    0,  210,    5}, // state   26
    { 215,   14,    0,  229,    5}, // state   27
    { 234,    1,    0,    0,    0}, // state   28
    {   0,    0,  235,    0,    0}, // state   29
    {   0,    0,  236,    0,    0}, // state   30
    { 237,   21,    0,    0,    0}, // state   31
    {   0,    0,  258,    0,    0}, // state   32
    {   0,    0,  259,    0,    0}, // state   33
    { 260,    1,    0,    0,    0}, // state   34
    {   0,    0,  261,    0,    0}, // state   35
    {   0,    0,  262,    0,    0}, // state   36
    { 263,    1,    0,    0,    0}, // state   37
    { 264,   21,    0,    0,    0}, // state   38
    {   0,    0,  285,    0,    0}, // state   39
    { 286,   26,    0,  312,    9}, // state   40
    { 321,    5,  326,    0,    0}, // state   41
    { 327,    5,  332,    0,    0}, // state   42
    { 333,    5,  338,    0,    0}, // state   43
    { 339,   14,    0,  353,    5}, // state   44
    {   0,    0,  358,    0,    0}, // state   45
    {   0,    0,  359,    0,    0}, // state   46
    {   0,    0,  360,    0,    0}, // state   47
    { 361,   21,    0,    0,    0}, // state   48
    { 382,   14,    0,  396,    5}, // state   49
    { 401,    1,    0,    0,    0}, // state   50
    { 402,    1,    0,    0,    0}, // state   51
    { 403,   15,    0,  418,    6}, // state   52
    { 424,    1,  425,    0,    0}, // state   53
    { 426,    2,  428,    0,    0}, // state   54
    { 429,    5,  434,    0,    0}, // state   55
    { 435,    5,  440,    0,    0}, // state   56
    { 441,    5,  446,    0,    0}, // state   57
    { 447,    5,  452,    0,    0}, // state   58
    { 453,    2,    0,    0,    0}, // state   59
    {   0,    0,  455,    0,    0}, // state   60
    { 456,   14,    0,  470,    5}, // state   61
    { 475,   14,    0,  489,    5}, // state   62
    { 494,   14,    0,  508,    5}, // state   63
    { 513,   14,    0,  527,    5}, // state   64
    { 532,   14,    0,  546,    5}, // state   65
    { 551,   14,    0,  565,    5}, // state   66
    { 570,   14,    0,  584,    5}, // state   67
    { 589,   14,    0,  603,    5}, // state   68
    { 608,   14,    0,  622,    5}, // state   69
    { 627,   14,    0,  641,    5}, // state   70
    { 646,   14,    0,  660,    5}, // state   71
    { 665,   14,    0,  679,    5}, // state   72
    { 684,   14,    0,  698,    5}, // state   73
    { 703,   14,    0,  717,    5}, // state   74
    { 722,   14,    0,  736,    5}, // state   75
    { 741,   14,    0,  755,    5}, // state   76
    { 760,   14,    0,  774,    5}, // state   77
    {   0,    0,  779,    0,    0}, // state   78
    {   0,    0,  780,    0,    0}, // state   79
    { 781,   14,    0,  795,    5}, // state   80
    { 800,   15,    0,  815,    6}, // state   81
    {   0,    0,  821,    0,    0}, // state   82
    {   0,    0,  822,    0,    0}, // state   83
    {   0,    0,  823,    0,    0}, // state   84
    { 824,   21,    0,    0,    0}, // state   85
    {   0,    0,  845,    0,    0}, // state   86
    { 846,   21,    0,    0,    0}, // state   87
    { 867,    2,    0,  869,    2}, // state   88
    { 871,    3,    0,  874,    2}, // state   89
    { 876,   15,    0,  891,    6}, // state   90
    { 897,   14,    0,  911,    5}, // state   91
    { 916,   14,    0,  930,    5}, // state   92
    { 935,   14,    0,  949,    5}, // state   93
    { 954,    1,    0,    0,    0}, // state   94
    { 955,    1,    0,    0,    0}, // state   95
    { 956,   20,  976,    0,    0}, // state   96
    { 977,   21,    0,    0,    0}, // state   97
    { 998,    8, 1006,    0,    0}, // state   98
    {1007,    8, 1015,    0,    0}, // state   99
    {1016,    5, 1021,    0,    0}, // state  100
    {1022,    5, 1027,    0,    0}, // state  101
    {1028,    4, 1032,    0,    0}, // state  102
    {1033,    5, 1038,    0,    0}, // state  103
    {1039,    4, 1043,    0,    0}, // state  104
    {1044,   11, 1055,    0,    0}, // state  105
    {1056,   11, 1067,    0,    0}, // state  106
    {1068,   15, 1083,    0,    0}, // state  107
    {1084,   15, 1099,    0,    0}, // state  108
    {1100,   11, 1111,    0,    0}, // state  109
    {1112,   11, 1123,    0,    0}, // state  110
    {1124,   17, 1141,    0,    0}, // state  111
    {1142,   18, 1160,    0,    0}, // state  112
    {1161,    8, 1169,    0,    0}, // state  113
    {   0,    0, 1170,    0,    0}, // state  114
    {1171,   20, 1191,    0,    0}, // state  115
    {1192,    2,    0,    0,    0}, // state  116
    {1194,   25,    0, 1219,    9}, // state  117
    {1228,   25,    0, 1253,    9}, // state  118
    {1262,    1,    0,    0,    0}, // state  119
    {1263,    2,    0,    0,    0}, // state  120
    {   0,    0, 1265,    0,    0}, // state  121
    {1266,    1,    0,    0,    0}, // state  122
    {1267,    2,    0,    0,    0}, // state  123
    {1269,    2,    0,    0,    0}, // state  124
    {1271,   15,    0, 1286,    5}, // state  125
    {1291,   20, 1311,    0,    0}, // state  126
    {1312,   20, 1332,    0,    0}, // state  127
    {1333,   21,    0,    0,    0}, // state  128
    {1354,    2,    0, 1356,    2}, // state  129
    {1358,    3,    0, 1361,    2}, // state  130
    {   0,    0, 1363,    0,    0}, // state  131
    {   0,    0, 1364,    0,    0}, // state  132
    {1365,   14,    0, 1379,    5}, // state  133
    {   0,    0, 1384,    0,    0}, // state  134
    {1385,    1, 1386,    0,    0}, // state  135
    {   0,    0, 1387, 1388,    1}, // state  136
    {1389,    1,    0,    0,    0}, // state  137
    {1390,    1,    0, 1391,    1}, // state  138
    {1392,    1,    0,    0,    0}, // state  139
    {   0,    0, 1393,    0,    0}, // state  140
    {   0,    0, 1394, 1395,    1}, // state  141
    {1396,    1,    0,    0,    0}, // state  142
    {1397,   25,    0, 1422,    9}, // state  143
    {1431,   21,    0,    0,    0}, // state  144
    {   0,    0, 1452,    0,    0}, // state  145
    {1453,    1,    0,    0,    0}, // state  146
    {1454,    2,    0,    0,    0}, // state  147
    {1456,    1,    0,    0,    0}, // state  148
    {1457,    2,    0,    0,    0}, // state  149
    {1459,    2,    0,    0,    0}, // state  150
    {1461,   20, 1481,    0,    0}, // state  151
    {1482,   25,    0, 1507,    9}, // state  152
    {1516,   26,    0, 1542,    9}, // state  153
    {   0,    0, 1551, 1552,    1}, // state  154
    {   0,    0, 1553,    0,    0}, // state  155
    {   0,    0, 1554, 1555,    1}, // state  156
    {1556,   27,    0, 1583,    9}, // state  157
    {   0,    0, 1592, 1593,    1}, // state  158
    {   0,    0, 1594,    0,    0}, // state  159
    {1595,   25,    0, 1620,    9}, // state  160
    {   0,    0, 1629, 1630,    1}, // state  161
    {1631,    1,    0,    0,    0}, // state  162
    {1632,    1,    0,    0,    0}, // state  163
    {   0,    0, 1633,    0,    0}, // state  164
    {   0,    0, 1634, 1635,    1}, // state  165
    {1636,    1,    0,    0,    0}, // state  166
    {   0,    0, 1637,    0,    0}, // state  167
    {   0,    0, 1638,    0,    0}, // state  168
    {1639,   26,    0, 1665,    9}, // state  169
    {1674,   26,    0, 1700,    9}, // state  170
    {   0,    0, 1709,    0,    0}, // state  171
    {   0,    0, 1710,    0,    0}, // state  172
    {1711,   27,    0, 1738,    9}, // state  173
    {   0,    0, 1747,    0,    0}, // state  174
    {1748,   26,    0, 1774,    9}, // state  175
    {   0,    0, 1783, 1784,    1}, // state  176
    {   0,    0, 1785, 1786,    1}, // state  177
    {1787,   27,    0, 1814,    9}, // state  178
    {   0,    0, 1823, 1824,    1}, // state  179
    {   0,    0, 1825,    0,    0}, // state  180
    {   0,    0, 1826,    0,    0}, // state  181
    {   0,    0, 1827,    0,    0}, // state  182
    {   0,    0, 1828,    0,    0}, // state  183
    {   0,    0, 1829,    0,    0}, // state  184
    {1830,   26,    0, 1856,    9}, // state  185
    {1865,   26,    0, 1891,    9}, // state  186
    {   0,    0, 1900,    0,    0}, // state  187
    {   0,    0, 1901,    0,    0}, // state  188
    {1902,   27,    0, 1929,    9}, // state  189
    {   0,    0, 1938,    0,    0}, // state  190
    {   0,    0, 1939,    0,    0}, // state  191
    {   0,    0, 1940,    0,    0}, // state  192
    {   0,    0, 1941,    0,    0}  // state  193

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
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
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
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    1}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state    3
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {TA_REDUCE_AND_ACCEPT_USING_RULE,    0}},

// ///////////////////////////////////////////////////////////////////////////
// state    4
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   72}},

// ///////////////////////////////////////////////////////////////////////////
// state    5
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state    6
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,   39}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state    7
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   41}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state    8
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   42}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state    9
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   43}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   10
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state   11
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state   12
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   46}},

// ///////////////////////////////////////////////////////////////////////////
// state   13
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   48}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   14
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   49}},

// ///////////////////////////////////////////////////////////////////////////
// state   15
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   50}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   51}},

// ///////////////////////////////////////////////////////////////////////////
// state   16
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   76}},

// ///////////////////////////////////////////////////////////////////////////
// state   17
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   75}},

// ///////////////////////////////////////////////////////////////////////////
// state   18
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   77}},

// ///////////////////////////////////////////////////////////////////////////
// state   19
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   52}},

// ///////////////////////////////////////////////////////////////////////////
// state   20
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    // nonterminal transitions
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   53}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   54}},

// ///////////////////////////////////////////////////////////////////////////
// state   21
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state   22
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   23
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   24
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   55}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   25
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   56}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   26
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   57}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   27
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   58}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   28
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   59}},

// ///////////////////////////////////////////////////////////////////////////
// state   29
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   25}},

// ///////////////////////////////////////////////////////////////////////////
// state   30
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   23}},

// ///////////////////////////////////////////////////////////////////////////
// state   31
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   32
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   24}},

// ///////////////////////////////////////////////////////////////////////////
// state   33
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   34
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   81}},

// ///////////////////////////////////////////////////////////////////////////
// state   35
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state   36
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   37
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   82}},

// ///////////////////////////////////////////////////////////////////////////
// state   38
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   39
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   27}},

// ///////////////////////////////////////////////////////////////////////////
// state   40
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   41
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   62}},

// ///////////////////////////////////////////////////////////////////////////
// state   42
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   63}},

// ///////////////////////////////////////////////////////////////////////////
// state   43
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   64}},

// ///////////////////////////////////////////////////////////////////////////
// state   44
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   85}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   45
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   46
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   47
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   33}},

// ///////////////////////////////////////////////////////////////////////////
// state   48
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   49
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   87}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   50
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   88}},

// ///////////////////////////////////////////////////////////////////////////
// state   51
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   89}},

// ///////////////////////////////////////////////////////////////////////////
// state   52
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   90}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   53
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   54
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   83}},

// ///////////////////////////////////////////////////////////////////////////
// state   55
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   67}},

// ///////////////////////////////////////////////////////////////////////////
// state   56
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   69}},

// ///////////////////////////////////////////////////////////////////////////
// state   57
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   65}},

// ///////////////////////////////////////////////////////////////////////////
// state   58
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   71}},

// ///////////////////////////////////////////////////////////////////////////
// state   59
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   95}},

// ///////////////////////////////////////////////////////////////////////////
// state   60
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   73}},

// ///////////////////////////////////////////////////////////////////////////
// state   61
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   96}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   62
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   97}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   63
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   98}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   64
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   99}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   65
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  100}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   66
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  101}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   67
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  102}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   68
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  103}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   69
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  104}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   70
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  105}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   71
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  106}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   72
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  107}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   73
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  108}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   74
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  109}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   75
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  110}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   76
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  111}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   77
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  112}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   78
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   68}},

// ///////////////////////////////////////////////////////////////////////////
// state   79
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   70}},

// ///////////////////////////////////////////////////////////////////////////
// state   80
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  113}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   81
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  114}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  115}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {           Token::param_list__, {                  TA_PUSH_STATE,  116}},

// ///////////////////////////////////////////////////////////////////////////
// state   82
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   28}},

// ///////////////////////////////////////////////////////////////////////////
// state   83
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   61}},

// ///////////////////////////////////////////////////////////////////////////
// state   84
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   26}},

// ///////////////////////////////////////////////////////////////////////////
// state   85
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  117}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   86
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   32}},

// ///////////////////////////////////////////////////////////////////////////
// state   87
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  118}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   88
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  119}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  120}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  121}},

// ///////////////////////////////////////////////////////////////////////////
// state   89
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  122}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  123}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  124}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  121}},

// ///////////////////////////////////////////////////////////////////////////
// state   90
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  125}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   91
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  126}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   92
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  127}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   93
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  128}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   94
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  129}},

// ///////////////////////////////////////////////////////////////////////////
// state   95
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  130}},

// ///////////////////////////////////////////////////////////////////////////
// state   96
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   50}},

// ///////////////////////////////////////////////////////////////////////////
// state   97
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  131}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   98
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state   99
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state  100
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   46}},

// ///////////////////////////////////////////////////////////////////////////
// state  101
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   47}},

// ///////////////////////////////////////////////////////////////////////////
// state  102
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   51}},

// ///////////////////////////////////////////////////////////////////////////
// state  103
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   48}},

// ///////////////////////////////////////////////////////////////////////////
// state  104
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   49}},

// ///////////////////////////////////////////////////////////////////////////
// state  105
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   57}},

// ///////////////////////////////////////////////////////////////////////////
// state  106
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   56}},

// ///////////////////////////////////////////////////////////////////////////
// state  107
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   54}},

// ///////////////////////////////////////////////////////////////////////////
// state  108
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   55}},

// ///////////////////////////////////////////////////////////////////////////
// state  109
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   59}},

// ///////////////////////////////////////////////////////////////////////////
// state  110
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   58}},

// ///////////////////////////////////////////////////////////////////////////
// state  111
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   53}},

// ///////////////////////////////////////////////////////////////////////////
// state  112
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   52}},

// ///////////////////////////////////////////////////////////////////////////
// state  113
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   60}},

// ///////////////////////////////////////////////////////////////////////////
// state  114
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   78}},

// ///////////////////////////////////////////////////////////////////////////
// state  115
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   85}},

// ///////////////////////////////////////////////////////////////////////////
// state  116
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  132}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  133}},

// ///////////////////////////////////////////////////////////////////////////
// state  117
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,  134}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  118
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,  135}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  119
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  136}},

// ///////////////////////////////////////////////////////////////////////////
// state  120
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  137}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  138}},

// ///////////////////////////////////////////////////////////////////////////
// state  121
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   20}},

// ///////////////////////////////////////////////////////////////////////////
// state  122
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  139}},

// ///////////////////////////////////////////////////////////////////////////
// state  123
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  140}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  141}},

// ///////////////////////////////////////////////////////////////////////////
// state  124
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  142}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  138}},

// ///////////////////////////////////////////////////////////////////////////
// state  125
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  143}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  144}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state  126
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   81}},

// ///////////////////////////////////////////////////////////////////////////
// state  127
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   84}},

// ///////////////////////////////////////////////////////////////////////////
// state  128
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  145}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state  129
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  146}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  147}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  121}},

// ///////////////////////////////////////////////////////////////////////////
// state  130
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  148}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  149}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  150}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  121}},

// ///////////////////////////////////////////////////////////////////////////
// state  131
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   66}},

// ///////////////////////////////////////////////////////////////////////////
// state  132
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state  133
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  151}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state  134
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   29}},

// ///////////////////////////////////////////////////////////////////////////
// state  135
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  152}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   31}},

// ///////////////////////////////////////////////////////////////////////////
// state  136
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  153}},

// ///////////////////////////////////////////////////////////////////////////
// state  137
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  154}},

// ///////////////////////////////////////////////////////////////////////////
// state  138
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,  155}},

// ///////////////////////////////////////////////////////////////////////////
// state  139
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  156}},

// ///////////////////////////////////////////////////////////////////////////
// state  140
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   16}},

// ///////////////////////////////////////////////////////////////////////////
// state  141
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  157}},

// ///////////////////////////////////////////////////////////////////////////
// state  142
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  158}},

// ///////////////////////////////////////////////////////////////////////////
// state  143
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,  159}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  144
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  160}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state  145
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   82}},

// ///////////////////////////////////////////////////////////////////////////
// state  146
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  161}},

// ///////////////////////////////////////////////////////////////////////////
// state  147
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  162}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  138}},

// ///////////////////////////////////////////////////////////////////////////
// state  148
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  163}},

// ///////////////////////////////////////////////////////////////////////////
// state  149
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  164}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  165}},

// ///////////////////////////////////////////////////////////////////////////
// state  150
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  166}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  138}},

// ///////////////////////////////////////////////////////////////////////////
// state  151
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   86}},

// ///////////////////////////////////////////////////////////////////////////
// state  152
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,  167}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  153
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  168}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  154
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  169}},

// ///////////////////////////////////////////////////////////////////////////
// state  155
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   21}},

// ///////////////////////////////////////////////////////////////////////////
// state  156
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  170}},

// ///////////////////////////////////////////////////////////////////////////
// state  157
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  171}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  172}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  158
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  173}},

// ///////////////////////////////////////////////////////////////////////////
// state  159
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   34}},

// ///////////////////////////////////////////////////////////////////////////
// state  160
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,  174}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  161
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  175}},

// ///////////////////////////////////////////////////////////////////////////
// state  162
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  176}},

// ///////////////////////////////////////////////////////////////////////////
// state  163
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  177}},

// ///////////////////////////////////////////////////////////////////////////
// state  164
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   17}},

// ///////////////////////////////////////////////////////////////////////////
// state  165
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  178}},

// ///////////////////////////////////////////////////////////////////////////
// state  166
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  179}},

// ///////////////////////////////////////////////////////////////////////////
// state  167
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   30}},

// ///////////////////////////////////////////////////////////////////////////
// state  168
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    5}},

// ///////////////////////////////////////////////////////////////////////////
// state  169
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  180}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  170
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  181}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  171
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   12}},

// ///////////////////////////////////////////////////////////////////////////
// state  172
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    9}},

// ///////////////////////////////////////////////////////////////////////////
// state  173
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  182}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  183}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  174
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   35}},

// ///////////////////////////////////////////////////////////////////////////
// state  175
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  184}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  176
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  185}},

// ///////////////////////////////////////////////////////////////////////////
// state  177
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  186}},

// ///////////////////////////////////////////////////////////////////////////
// state  178
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  187}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  188}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  179
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  189}},

// ///////////////////////////////////////////////////////////////////////////
// state  180
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    7}},

// ///////////////////////////////////////////////////////////////////////////
// state  181
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    3}},

// ///////////////////////////////////////////////////////////////////////////
// state  182
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},

// ///////////////////////////////////////////////////////////////////////////
// state  183
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    2}},

// ///////////////////////////////////////////////////////////////////////////
// state  184
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    6}},

// ///////////////////////////////////////////////////////////////////////////
// state  185
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  190}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  186
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  191}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  187
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},

// ///////////////////////////////////////////////////////////////////////////
// state  188
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},

// ///////////////////////////////////////////////////////////////////////////
// state  189
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  192}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  193}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   29}},
    {            Token::statement__, {                  TA_PUSH_STATE,   30}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  190
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    8}},

// ///////////////////////////////////////////////////////////////////////////
// state  191
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    4}},

// ///////////////////////////////////////////////////////////////////////////
// state  192
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   15}},

// ///////////////////////////////////////////////////////////////////////////
// state  193
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}}

};

unsigned int const SteelParser::ms_state_transition_count =
    sizeof(SteelParser::ms_state_transition) /
    sizeof(SteelParser::StateTransition);


#line 48 "steel.trison"


void SteelParser::addError(unsigned int line, const std::string &error)
{
	mbErrorEncountered = true;
	mErrors +=  GET_SCRIPT() + ':' + itos(line) + ": " + error + '\n';
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

#line 5071 "SteelParser.cpp"

