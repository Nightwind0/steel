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


#line 27 "SteelParser.cpp"

SteelParser::SteelParser ()

{

#line 69 "steel.trison"

    m_scanner = new SteelScanner();

#line 37 "SteelParser.cpp"
    m_debug_spew_level = 0;
    DEBUG_SPEW_2("### number of state transitions = " << ms_state_transition_count << std::endl);
    m_reduction_token = static_cast<AstBase*>(0);
}

SteelParser::~SteelParser ()
{

#line 73 "steel.trison"

    delete m_scanner;

#line 50 "SteelParser.cpp"
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

#line 42 "steel.trison"

	mbErrorEncountered = false;
	mErrors.clear();

#line 92 "SteelParser.cpp"

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


#line 78 "steel.trison"

    delete token;

#line 393 "SteelParser.cpp"
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

#line 130 "steel.trison"

				AstScript * pScript =   new AstScript(
							list->GetLine(),
							list->GetScript());
			        pScript->SetList(list);
				return pScript;
			
#line 507 "SteelParser.cpp"
}

// rule 2: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' param_definition:params ')' '{':b1 statement_list:stmts '}':b2    
AstBase* SteelParser::ReductionRuleHandler0002 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(5 < m_reduction_rule_token_count);
    AstBase* b1 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);
    assert(7 < m_reduction_rule_token_count);
    AstBase* b2 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 143 "steel.trison"

					delete b1;
					delete b2;
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									false);
				
#line 535 "SteelParser.cpp"
}

// rule 3: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' %error ')' '{':b1 statement_list:stmts '}':b2    
AstBase* SteelParser::ReductionRuleHandler0003 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(5 < m_reduction_rule_token_count);
    AstBase* b1 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);
    assert(7 < m_reduction_rule_token_count);
    AstBase* b2 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 155 "steel.trison"

					delete b1;
					delete b2;
					AstFuncIdentifier *pId = static_cast<AstFuncIdentifier*>(id);
					addError(id->GetLine(),"parser error in parameter list for function '" + pId->getValue() + '\'');
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									pId,
									NULL,
									stmts,
									false);
				
#line 563 "SteelParser.cpp"
}

// rule 4: func_definition <- FUNCTION FUNC_IDENTIFIER:id '(' ')' '{':b1 statement_list:stmts '}':b2    
AstBase* SteelParser::ReductionRuleHandler0004 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(4 < m_reduction_rule_token_count);
    AstBase* b1 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(5 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);
    assert(6 < m_reduction_rule_token_count);
    AstBase* b2 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 169 "steel.trison"

					delete b1;
					delete b2;
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									NULL,
									stmts,
									false);
				
#line 589 "SteelParser.cpp"
}

// rule 5: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' param_definition:params ')' '{':b1 statement_list:stmts '}':b2    
AstBase* SteelParser::ReductionRuleHandler0005 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstBase* b1 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);
    assert(8 < m_reduction_rule_token_count);
    AstBase* b2 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 8]);

#line 181 "steel.trison"

					delete b1;
					delete b2;
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									true);
				
#line 617 "SteelParser.cpp"
}

// rule 6: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER:id '(' ')' '{':b1 statement_list:stmts '}':b2    
AstBase* SteelParser::ReductionRuleHandler0006 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(5 < m_reduction_rule_token_count);
    AstBase* b1 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);
    assert(7 < m_reduction_rule_token_count);
    AstBase* b2 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 193 "steel.trison"

					delete b1;
					delete b2;
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									NULL,
									stmts,
									true);
				
#line 643 "SteelParser.cpp"
}

// rule 7: param_id <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0007 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 208 "steel.trison"
 return id; 
#line 654 "SteelParser.cpp"
}

// rule 8: param_id <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0008 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 210 "steel.trison"
 return id; 
#line 665 "SteelParser.cpp"
}

// rule 9: param_definition <- vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0009 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 216 "steel.trison"

				AstParamDefinitionList * pList = new AstParamDefinitionList(decl->GetLine(),decl->GetScript());
				pList->add(static_cast<AstDeclaration*>(decl));
				return pList;		
			
#line 680 "SteelParser.cpp"
}

// rule 10: param_definition <- param_definition:list ',' vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0010 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamDefinitionList* list = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 223 "steel.trison"

				list->add(static_cast<AstDeclaration*>(decl));
				return list;
			
#line 696 "SteelParser.cpp"
}

// rule 11: statement_list <-     
AstBase* SteelParser::ReductionRuleHandler0011 ()
{

#line 231 "steel.trison"

				AstStatementList *pList = 
					new AstStatementList(m_scanner->getCurrentLine(),
										m_scanner->getScriptName());
				return pList;
			
#line 710 "SteelParser.cpp"
}

// rule 12: statement_list <- statement_list:list statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0012 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 239 "steel.trison"

					list->add( stmt ); 
					return list;
				
#line 726 "SteelParser.cpp"
}

// rule 13: statement <- exp_statement:exp    
AstBase* SteelParser::ReductionRuleHandler0013 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 247 "steel.trison"
 return new AstExpressionStatement(exp->GetLine(),exp->GetScript(), exp); 
#line 737 "SteelParser.cpp"
}

// rule 14: statement <- func_definition:func    
AstBase* SteelParser::ReductionRuleHandler0014 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFunctionDefinition* func = static_cast< AstFunctionDefinition* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 249 "steel.trison"
 return func; 
#line 748 "SteelParser.cpp"
}

// rule 15: statement <- '{':b1 statement_list:list '}':b2    
AstBase* SteelParser::ReductionRuleHandler0015 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* b1 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(2 < m_reduction_rule_token_count);
    AstBase* b2 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 251 "steel.trison"
 delete b1; delete b2; return list; 
#line 763 "SteelParser.cpp"
}

// rule 16: statement <- '{':b1 '}':b2    
AstBase* SteelParser::ReductionRuleHandler0016 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* b1 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstBase* b2 = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 253 "steel.trison"

			 int line = b1->GetLine();
			 std::string script = b1->GetScript();
			 delete b1;
			 delete b2;
			 return new AstStatement(line,script);
			
#line 782 "SteelParser.cpp"
}

// rule 17: statement <- vardecl:vardecl ';':semi    
AstBase* SteelParser::ReductionRuleHandler0017 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* vardecl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstBase* semi = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 261 "steel.trison"
 delete semi; return vardecl; 
#line 795 "SteelParser.cpp"
}

// rule 18: statement <- WHILE '(' exp:exp ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0018 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 263 "steel.trison"
 return new AstWhileStatement(exp->GetLine(), exp->GetScript(),exp,stmt); 
#line 808 "SteelParser.cpp"
}

// rule 19: statement <- IF '(' exp:exp ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0019 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 265 "steel.trison"
 return new AstIfStatement(exp->GetLine(), exp->GetScript(),exp,stmt,elses);
#line 823 "SteelParser.cpp"
}

// rule 20: statement <- IF '(' exp:exp ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0020 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 267 "steel.trison"
 return new AstIfStatement(exp->GetLine(),exp->GetScript(),exp,stmt); 
#line 836 "SteelParser.cpp"
}

// rule 21: statement <- RETURN exp:exp ';':semi    
AstBase* SteelParser::ReductionRuleHandler0021 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(2 < m_reduction_rule_token_count);
    AstBase* semi = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 269 "steel.trison"
 delete semi; return new AstReturnStatement(exp->GetLine(),exp->GetScript(),exp);
#line 849 "SteelParser.cpp"
}

// rule 22: statement <- RETURN ';':semi    
AstBase* SteelParser::ReductionRuleHandler0022 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* semi = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 272 "steel.trison"

				int line = semi->GetLine();
				std::string script = semi->GetScript();
				delete semi;
				return new AstReturnStatement(line,script);
			
#line 865 "SteelParser.cpp"
}

// rule 23: statement <- FOR '(' exp_statement:start exp_statement:condition ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0023 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(5 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);

#line 280 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition, 
						new AstExpression (start->GetLine(),start->GetScript()), stmt); 
			
#line 883 "SteelParser.cpp"
}

// rule 24: statement <- FOR '(' exp_statement:start exp_statement:condition exp:iteration ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0024 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* iteration = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 286 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition,iteration,stmt);
			
#line 902 "SteelParser.cpp"
}

// rule 25: statement <- BREAK ';':semi    
AstBase* SteelParser::ReductionRuleHandler0025 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* semi = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 291 "steel.trison"

				int line = semi->GetLine();
				std::string script = semi->GetScript();
				delete semi;
				return new AstBreakStatement(line,script); 
			
#line 918 "SteelParser.cpp"
}

// rule 26: statement <- CONTINUE ';':semi    
AstBase* SteelParser::ReductionRuleHandler0026 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstBase* semi = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 299 "steel.trison"

				int line = semi->GetLine();
				std::string script = semi->GetScript();
				delete semi;
				return new AstContinueStatement(line,script); 
			
#line 934 "SteelParser.cpp"
}

// rule 27: exp <- call:call    
AstBase* SteelParser::ReductionRuleHandler0027 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstCallExpression* call = static_cast< AstCallExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 310 "steel.trison"
 return call; 
#line 945 "SteelParser.cpp"
}

// rule 28: exp <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0028 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 312 "steel.trison"
 return i;
#line 956 "SteelParser.cpp"
}

// rule 29: exp <- FLOAT:f    
AstBase* SteelParser::ReductionRuleHandler0029 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* f = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 314 "steel.trison"
 return f; 
#line 967 "SteelParser.cpp"
}

// rule 30: exp <- STRING:s    
AstBase* SteelParser::ReductionRuleHandler0030 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* s = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 316 "steel.trison"
 return s; 
#line 978 "SteelParser.cpp"
}

// rule 31: exp <- var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0031 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 318 "steel.trison"
 return id; 
#line 989 "SteelParser.cpp"
}

// rule 32: exp <- array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0032 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 320 "steel.trison"
 return id; 
#line 1000 "SteelParser.cpp"
}

// rule 33: exp <- exp:a '+' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0033 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 322 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,b); 
#line 1013 "SteelParser.cpp"
}

// rule 34: exp <- exp:a '-' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0034 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 324 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,b); 
#line 1026 "SteelParser.cpp"
}

// rule 35: exp <- exp:a '*' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0035 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 326 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,b); 
#line 1039 "SteelParser.cpp"
}

// rule 36: exp <- exp:a '/' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0036 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 328 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::DIV,a,b); 
#line 1052 "SteelParser.cpp"
}

// rule 37: exp <- exp:a '%' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0037 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 330 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MOD,a,b); 
#line 1065 "SteelParser.cpp"
}

// rule 38: exp <- exp:a D exp:b    %left %prec POW
AstBase* SteelParser::ReductionRuleHandler0038 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 332 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::D,a,b); 
#line 1078 "SteelParser.cpp"
}

// rule 39: exp <- exp:lvalue '=' exp:exp    %right %prec ASSIGNMENT
AstBase* SteelParser::ReductionRuleHandler0039 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 334 "steel.trison"
 return new AstVarAssignmentExpression(lvalue->GetLine(),lvalue->GetScript(),lvalue,exp); 
#line 1091 "SteelParser.cpp"
}

// rule 40: exp <- exp:a '^' exp:b    %right %prec POW
AstBase* SteelParser::ReductionRuleHandler0040 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 336 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::POW,a,b); 
#line 1104 "SteelParser.cpp"
}

// rule 41: exp <- exp:a OR exp:b    %left %prec OR
AstBase* SteelParser::ReductionRuleHandler0041 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 338 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::OR,a,b); 
#line 1117 "SteelParser.cpp"
}

// rule 42: exp <- exp:a AND exp:b    %left %prec AND
AstBase* SteelParser::ReductionRuleHandler0042 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 340 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::AND,a,b); 
#line 1130 "SteelParser.cpp"
}

// rule 43: exp <- exp:a EQ exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0043 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 342 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::EQ,a,b); 
#line 1143 "SteelParser.cpp"
}

// rule 44: exp <- exp:a NE exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0044 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 344 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::NE,a,b); 
#line 1156 "SteelParser.cpp"
}

// rule 45: exp <- exp:a LT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0045 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 346 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LT,a,b); 
#line 1169 "SteelParser.cpp"
}

// rule 46: exp <- exp:a GT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0046 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 348 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GT,a,b); 
#line 1182 "SteelParser.cpp"
}

// rule 47: exp <- exp:a LTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0047 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 350 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LTE,a,b); 
#line 1195 "SteelParser.cpp"
}

// rule 48: exp <- exp:a GTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0048 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 352 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GTE,a,b); 
#line 1208 "SteelParser.cpp"
}

// rule 49: exp <- exp:a CAT exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0049 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 354 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::CAT,a,b); 
#line 1221 "SteelParser.cpp"
}

// rule 50: exp <- '(' exp:exp ')'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0050 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 356 "steel.trison"
 return exp; 
#line 1232 "SteelParser.cpp"
}

// rule 51: exp <- '-' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0051 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 358 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::MINUS,exp); 
#line 1243 "SteelParser.cpp"
}

// rule 52: exp <- '+' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0052 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 360 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::PLUS,exp); 
#line 1254 "SteelParser.cpp"
}

// rule 53: exp <- NOT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0053 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 362 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::NOT,exp); 
#line 1265 "SteelParser.cpp"
}

// rule 54: exp <- CAT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0054 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 364 "steel.trison"
 return new AstUnaryOp(exp->GetLine(),
exp->GetScript(), AstUnaryOp::CAT,exp); 
#line 1277 "SteelParser.cpp"
}

// rule 55: exp <- exp:lvalue '[' exp:index ']'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0055 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* index = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 367 "steel.trison"
 return new AstArrayElement(lvalue->GetLine(),lvalue->GetScript(),lvalue,index); 
#line 1290 "SteelParser.cpp"
}

// rule 56: exp <- INCREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0056 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 369 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::PRE);
#line 1301 "SteelParser.cpp"
}

// rule 57: exp <- exp:lvalue INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0057 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 371 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::POST);
#line 1312 "SteelParser.cpp"
}

// rule 58: exp <- DECREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0058 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 373 "steel.trison"
 return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::PRE);
#line 1323 "SteelParser.cpp"
}

// rule 59: exp <- exp:lvalue DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0059 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 375 "steel.trison"
 return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::POST);
#line 1334 "SteelParser.cpp"
}

// rule 60: exp <- POP exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0060 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 377 "steel.trison"
 return new AstPop(lvalue->GetLine(),lvalue->GetScript(),lvalue); 
#line 1345 "SteelParser.cpp"
}

// rule 61: exp_statement <- ';':semi    
AstBase* SteelParser::ReductionRuleHandler0061 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* semi = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 382 "steel.trison"

			int line = semi->GetLine();
			std::string script = semi->GetScript(); 
			delete semi;
			return new AstExpression(line,script); 
		
#line 1361 "SteelParser.cpp"
}

// rule 62: exp_statement <- exp:exp ';':semi    
AstBase* SteelParser::ReductionRuleHandler0062 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstBase* semi = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 389 "steel.trison"
 delete semi;  return exp; 
#line 1374 "SteelParser.cpp"
}

// rule 63: int_literal <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0063 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 394 "steel.trison"
 return i; 
#line 1385 "SteelParser.cpp"
}

// rule 64: var_identifier <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0064 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 399 "steel.trison"
 return id; 
#line 1396 "SteelParser.cpp"
}

// rule 65: func_identifier <- FUNC_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0065 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 404 "steel.trison"
 return id; 
#line 1407 "SteelParser.cpp"
}

// rule 66: array_identifier <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0066 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 409 "steel.trison"
 return id; 
#line 1418 "SteelParser.cpp"
}

// rule 67: call <- func_identifier:id '(' ')'    
AstBase* SteelParser::ReductionRuleHandler0067 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 415 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id);
#line 1429 "SteelParser.cpp"
}

// rule 68: call <- func_identifier:id '(' param_list:params ')'    
AstBase* SteelParser::ReductionRuleHandler0068 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 417 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id,params);
#line 1442 "SteelParser.cpp"
}

// rule 69: vardecl <- VAR var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0069 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 422 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id);
#line 1453 "SteelParser.cpp"
}

// rule 70: vardecl <- VAR var_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0070 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 424 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id,exp); 
#line 1466 "SteelParser.cpp"
}

// rule 71: vardecl <- VAR array_identifier:id '[' exp:i ']'    
AstBase* SteelParser::ReductionRuleHandler0071 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* i = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 426 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id,i); 
#line 1479 "SteelParser.cpp"
}

// rule 72: vardecl <- VAR array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0072 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 428 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id); 
#line 1490 "SteelParser.cpp"
}

// rule 73: vardecl <- VAR array_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0073 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 430 "steel.trison"

							AstArrayDeclaration *pDecl =  new AstArrayDeclaration(id->GetLine(),id->GetScript(),id);
							pDecl->assign(exp);
							return pDecl;
						 
#line 1507 "SteelParser.cpp"
}

// rule 74: param_list <- exp:exp    
AstBase* SteelParser::ReductionRuleHandler0074 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 439 "steel.trison"
 AstParamList * pList = new AstParamList ( exp->GetLine(), exp->GetScript() );
		  pList->add(exp);
		  return pList;
		
#line 1521 "SteelParser.cpp"
}

// rule 75: param_list <- param_list:list ',' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0075 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamList* list = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 444 "steel.trison"
 list->add(exp); return list;
#line 1534 "SteelParser.cpp"
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
    {      Token::func_definition__,  7, &SteelParser::ReductionRuleHandler0004, "rule 4: func_definition <- FUNCTION FUNC_IDENTIFIER '(' ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  9, &SteelParser::ReductionRuleHandler0005, "rule 5: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER '(' param_definition ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0006, "rule 6: func_definition <- FINAL FUNCTION FUNC_IDENTIFIER '(' ')' '{' statement_list '}'    "},
    {             Token::param_id__,  1, &SteelParser::ReductionRuleHandler0007, "rule 7: param_id <- VAR_IDENTIFIER    "},
    {             Token::param_id__,  1, &SteelParser::ReductionRuleHandler0008, "rule 8: param_id <- ARRAY_IDENTIFIER    "},
    {     Token::param_definition__,  1, &SteelParser::ReductionRuleHandler0009, "rule 9: param_definition <- vardecl    "},
    {     Token::param_definition__,  3, &SteelParser::ReductionRuleHandler0010, "rule 10: param_definition <- param_definition ',' vardecl    "},
    {       Token::statement_list__,  0, &SteelParser::ReductionRuleHandler0011, "rule 11: statement_list <-     "},
    {       Token::statement_list__,  2, &SteelParser::ReductionRuleHandler0012, "rule 12: statement_list <- statement_list statement    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0013, "rule 13: statement <- exp_statement    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0014, "rule 14: statement <- func_definition    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0015, "rule 15: statement <- '{' statement_list '}'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0016, "rule 16: statement <- '{' '}'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0017, "rule 17: statement <- vardecl ';'    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0018, "rule 18: statement <- WHILE '(' exp ')' statement    "},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0019, "rule 19: statement <- IF '(' exp ')' statement ELSE statement     %prec ELSE"},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0020, "rule 20: statement <- IF '(' exp ')' statement     %prec NON_ELSE"},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0021, "rule 21: statement <- RETURN exp ';'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0022, "rule 22: statement <- RETURN ';'    "},
    {            Token::statement__,  6, &SteelParser::ReductionRuleHandler0023, "rule 23: statement <- FOR '(' exp_statement exp_statement ')' statement    "},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0024, "rule 24: statement <- FOR '(' exp_statement exp_statement exp ')' statement    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0025, "rule 25: statement <- BREAK ';'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0026, "rule 26: statement <- CONTINUE ';'    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0027, "rule 27: exp <- call    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0028, "rule 28: exp <- INT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0029, "rule 29: exp <- FLOAT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0030, "rule 30: exp <- STRING    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0031, "rule 31: exp <- var_identifier    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0032, "rule 32: exp <- array_identifier    "},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0033, "rule 33: exp <- exp '+' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0034, "rule 34: exp <- exp '-' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0035, "rule 35: exp <- exp '*' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0036, "rule 36: exp <- exp '/' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0037, "rule 37: exp <- exp '%' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0038, "rule 38: exp <- exp D exp    %left %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0039, "rule 39: exp <- exp '=' exp    %right %prec ASSIGNMENT"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0040, "rule 40: exp <- exp '^' exp    %right %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0041, "rule 41: exp <- exp OR exp    %left %prec OR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0042, "rule 42: exp <- exp AND exp    %left %prec AND"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0043, "rule 43: exp <- exp EQ exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0044, "rule 44: exp <- exp NE exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0045, "rule 45: exp <- exp LT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0046, "rule 46: exp <- exp GT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0047, "rule 47: exp <- exp LTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0048, "rule 48: exp <- exp GTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0049, "rule 49: exp <- exp CAT exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0050, "rule 50: exp <- '(' exp ')'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0051, "rule 51: exp <- '-' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0052, "rule 52: exp <- '+' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0053, "rule 53: exp <- NOT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0054, "rule 54: exp <- CAT exp     %prec UNARY"},
    {                  Token::exp__,  4, &SteelParser::ReductionRuleHandler0055, "rule 55: exp <- exp '[' exp ']'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0056, "rule 56: exp <- INCREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0057, "rule 57: exp <- exp INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0058, "rule 58: exp <- DECREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0059, "rule 59: exp <- exp DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0060, "rule 60: exp <- POP exp     %prec UNARY"},
    {        Token::exp_statement__,  1, &SteelParser::ReductionRuleHandler0061, "rule 61: exp_statement <- ';'    "},
    {        Token::exp_statement__,  2, &SteelParser::ReductionRuleHandler0062, "rule 62: exp_statement <- exp ';'    "},
    {          Token::int_literal__,  1, &SteelParser::ReductionRuleHandler0063, "rule 63: int_literal <- INT    "},
    {       Token::var_identifier__,  1, &SteelParser::ReductionRuleHandler0064, "rule 64: var_identifier <- VAR_IDENTIFIER    "},
    {      Token::func_identifier__,  1, &SteelParser::ReductionRuleHandler0065, "rule 65: func_identifier <- FUNC_IDENTIFIER    "},
    {     Token::array_identifier__,  1, &SteelParser::ReductionRuleHandler0066, "rule 66: array_identifier <- ARRAY_IDENTIFIER    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0067, "rule 67: call <- func_identifier '(' ')'    "},
    {                 Token::call__,  4, &SteelParser::ReductionRuleHandler0068, "rule 68: call <- func_identifier '(' param_list ')'    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0069, "rule 69: vardecl <- VAR var_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0070, "rule 70: vardecl <- VAR var_identifier '=' exp    "},
    {              Token::vardecl__,  5, &SteelParser::ReductionRuleHandler0071, "rule 71: vardecl <- VAR array_identifier '[' exp ']'    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0072, "rule 72: vardecl <- VAR array_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0073, "rule 73: vardecl <- VAR array_identifier '=' exp    "},
    {           Token::param_list__,  1, &SteelParser::ReductionRuleHandler0074, "rule 74: param_list <- exp    "},
    {           Token::param_list__,  3, &SteelParser::ReductionRuleHandler0075, "rule 75: param_list <- param_list ',' exp    "},

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
    { 145,    1,    0,    0,    0}, // state   15
    {   0,    0,  146,    0,    0}, // state   16
    {   0,    0,  147,    0,    0}, // state   17
    {   0,    0,  148,    0,    0}, // state   18
    { 149,    1,    0,    0,    0}, // state   19
    { 150,    2,    0,  152,    2}, // state   20
    {   0,    0,  154,    0,    0}, // state   21
    {   0,    0,  155,    0,    0}, // state   22
    {   0,    0,  156,    0,    0}, // state   23
    { 157,   14,    0,  171,    5}, // state   24
    { 176,   14,    0,  190,    5}, // state   25
    { 195,   14,    0,  209,    5}, // state   26
    { 214,   14,    0,  228,    5}, // state   27
    { 233,    1,    0,    0,    0}, // state   28
    {   0,    0,  234,    0,    0}, // state   29
    {   0,    0,  235,    0,    0}, // state   30
    { 236,   21,    0,    0,    0}, // state   31
    {   0,    0,  257,    0,    0}, // state   32
    {   0,    0,  258,    0,    0}, // state   33
    { 259,    1,    0,    0,    0}, // state   34
    {   0,    0,  260,    0,    0}, // state   35
    {   0,    0,  261,    0,    0}, // state   36
    { 262,    1,    0,    0,    0}, // state   37
    { 263,   21,    0,    0,    0}, // state   38
    {   0,    0,  284,    0,    0}, // state   39
    { 285,   26,    0,  311,    9}, // state   40
    { 320,    5,  325,    0,    0}, // state   41
    { 326,    5,  331,    0,    0}, // state   42
    { 332,    5,  337,    0,    0}, // state   43
    { 338,   14,    0,  352,    5}, // state   44
    {   0,    0,  357,    0,    0}, // state   45
    {   0,    0,  358,    0,    0}, // state   46
    {   0,    0,  359,    0,    0}, // state   47
    { 360,   21,    0,    0,    0}, // state   48
    { 381,   14,    0,  395,    5}, // state   49
    { 400,    1,    0,    0,    0}, // state   50
    { 401,   15,    0,  416,    6}, // state   51
    { 422,    1,  423,    0,    0}, // state   52
    { 424,    2,  426,    0,    0}, // state   53
    { 427,    5,  432,    0,    0}, // state   54
    { 433,    5,  438,    0,    0}, // state   55
    { 439,    5,  444,    0,    0}, // state   56
    { 445,    5,  450,    0,    0}, // state   57
    { 451,    1,    0,    0,    0}, // state   58
    {   0,    0,  452,    0,    0}, // state   59
    { 453,   14,    0,  467,    5}, // state   60
    { 472,   14,    0,  486,    5}, // state   61
    { 491,   14,    0,  505,    5}, // state   62
    { 510,   14,    0,  524,    5}, // state   63
    { 529,   14,    0,  543,    5}, // state   64
    { 548,   14,    0,  562,    5}, // state   65
    { 567,   14,    0,  581,    5}, // state   66
    { 586,   14,    0,  600,    5}, // state   67
    { 605,   14,    0,  619,    5}, // state   68
    { 624,   14,    0,  638,    5}, // state   69
    { 643,   14,    0,  657,    5}, // state   70
    { 662,   14,    0,  676,    5}, // state   71
    { 681,   14,    0,  695,    5}, // state   72
    { 700,   14,    0,  714,    5}, // state   73
    { 719,   14,    0,  733,    5}, // state   74
    { 738,   14,    0,  752,    5}, // state   75
    { 757,   14,    0,  771,    5}, // state   76
    {   0,    0,  776,    0,    0}, // state   77
    {   0,    0,  777,    0,    0}, // state   78
    { 778,   14,    0,  792,    5}, // state   79
    { 797,   15,    0,  812,    6}, // state   80
    {   0,    0,  818,    0,    0}, // state   81
    {   0,    0,  819,    0,    0}, // state   82
    {   0,    0,  820,    0,    0}, // state   83
    { 821,   21,    0,    0,    0}, // state   84
    {   0,    0,  842,    0,    0}, // state   85
    { 843,   21,    0,    0,    0}, // state   86
    { 864,    3,    0,  867,    2}, // state   87
    { 869,   15,    0,  884,    6}, // state   88
    { 890,   14,    0,  904,    5}, // state   89
    { 909,   14,    0,  923,    5}, // state   90
    { 928,   14,    0,  942,    5}, // state   91
    { 947,    1,    0,    0,    0}, // state   92
    { 948,   20,  968,    0,    0}, // state   93
    { 969,   21,    0,    0,    0}, // state   94
    { 990,    8,  998,    0,    0}, // state   95
    { 999,    8, 1007,    0,    0}, // state   96
    {1008,    5, 1013,    0,    0}, // state   97
    {1014,    5, 1019,    0,    0}, // state   98
    {1020,    4, 1024,    0,    0}, // state   99
    {1025,    5, 1030,    0,    0}, // state  100
    {1031,    4, 1035,    0,    0}, // state  101
    {1036,   11, 1047,    0,    0}, // state  102
    {1048,   11, 1059,    0,    0}, // state  103
    {1060,   15, 1075,    0,    0}, // state  104
    {1076,   15, 1091,    0,    0}, // state  105
    {1092,   11, 1103,    0,    0}, // state  106
    {1104,   11, 1115,    0,    0}, // state  107
    {1116,   17, 1133,    0,    0}, // state  108
    {1134,   18, 1152,    0,    0}, // state  109
    {1153,    8, 1161,    0,    0}, // state  110
    {   0,    0, 1162,    0,    0}, // state  111
    {1163,   20, 1183,    0,    0}, // state  112
    {1184,    2,    0,    0,    0}, // state  113
    {1186,   25,    0, 1211,    9}, // state  114
    {1220,   25,    0, 1245,    9}, // state  115
    {1254,    1,    0,    0,    0}, // state  116
    {1255,    1,    0,    0,    0}, // state  117
    {1256,    2,    0,    0,    0}, // state  118
    {   0,    0, 1258,    0,    0}, // state  119
    {1259,   15,    0, 1274,    5}, // state  120
    {1279,   20, 1299,    0,    0}, // state  121
    {1300,   20, 1320,    0,    0}, // state  122
    {1321,   21,    0,    0,    0}, // state  123
    {1342,    2,    0, 1344,    2}, // state  124
    {   0,    0, 1346,    0,    0}, // state  125
    {   0,    0, 1347,    0,    0}, // state  126
    {1348,   14,    0, 1362,    5}, // state  127
    {   0,    0, 1367,    0,    0}, // state  128
    {1368,    1, 1369,    0,    0}, // state  129
    {1370,    1,    0,    0,    0}, // state  130
    {   0,    0, 1371, 1372,    1}, // state  131
    {1373,    1,    0,    0,    0}, // state  132
    {1374,    1,    0, 1375,    1}, // state  133
    {1376,   25,    0, 1401,    9}, // state  134
    {1410,   21,    0,    0,    0}, // state  135
    {   0,    0, 1431,    0,    0}, // state  136
    {1432,    1,    0,    0,    0}, // state  137
    {1433,    2,    0,    0,    0}, // state  138
    {1435,   20, 1455,    0,    0}, // state  139
    {1456,   25,    0, 1481,    9}, // state  140
    {   0,    0, 1490, 1491,    1}, // state  141
    {1492,   26,    0, 1518,    9}, // state  142
    {   0,    0, 1527, 1528,    1}, // state  143
    {   0,    0, 1529,    0,    0}, // state  144
    {   0,    0, 1530,    0,    0}, // state  145
    {1531,   25,    0, 1556,    9}, // state  146
    {   0,    0, 1565, 1566,    1}, // state  147
    {1567,    1,    0,    0,    0}, // state  148
    {   0,    0, 1568,    0,    0}, // state  149
    {1569,   26,    0, 1595,    9}, // state  150
    {   0,    0, 1604,    0,    0}, // state  151
    {1605,   26,    0, 1631,    9}, // state  152
    {   0,    0, 1640,    0,    0}, // state  153
    {1641,   26,    0, 1667,    9}, // state  154
    {   0,    0, 1676, 1677,    1}, // state  155
    {   0,    0, 1678,    0,    0}, // state  156
    {   0,    0, 1679,    0,    0}, // state  157
    {   0,    0, 1680,    0,    0}, // state  158
    {1681,   26,    0, 1707,    9}, // state  159
    {   0,    0, 1716,    0,    0}  // state  160

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
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},
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
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   61}},

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
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},
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
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   50}},

// ///////////////////////////////////////////////////////////////////////////
// state   16
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   65}},

// ///////////////////////////////////////////////////////////////////////////
// state   17
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   64}},

// ///////////////////////////////////////////////////////////////////////////
// state   18
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   66}},

// ///////////////////////////////////////////////////////////////////////////
// state   19
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   51}},

// ///////////////////////////////////////////////////////////////////////////
// state   20
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    // nonterminal transitions
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   52}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   53}},

// ///////////////////////////////////////////////////////////////////////////
// state   21
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   28}},

// ///////////////////////////////////////////////////////////////////////////
// state   22
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   29}},

// ///////////////////////////////////////////////////////////////////////////
// state   23
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   30}},

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
    {                  Token::exp__, {                  TA_PUSH_STATE,   54}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   55}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   56}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   57}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   28
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   58}},

// ///////////////////////////////////////////////////////////////////////////
// state   29
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},

// ///////////////////////////////////////////////////////////////////////////
// state   30
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   12}},

// ///////////////////////////////////////////////////////////////////////////
// state   31
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   59}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state   32
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},

// ///////////////////////////////////////////////////////////////////////////
// state   33
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   31}},

// ///////////////////////////////////////////////////////////////////////////
// state   34
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state   35
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   32}},

// ///////////////////////////////////////////////////////////////////////////
// state   36
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   27}},

// ///////////////////////////////////////////////////////////////////////////
// state   37
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   81}},

// ///////////////////////////////////////////////////////////////////////////
// state   38
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state   39
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   16}},

// ///////////////////////////////////////////////////////////////////////////
// state   40
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
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
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   51}},

// ///////////////////////////////////////////////////////////////////////////
// state   42
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   52}},

// ///////////////////////////////////////////////////////////////////////////
// state   43
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   53}},

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
    {                  Token::exp__, {                  TA_PUSH_STATE,   84}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   45
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   25}},

// ///////////////////////////////////////////////////////////////////////////
// state   46
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   26}},

// ///////////////////////////////////////////////////////////////////////////
// state   47
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},

// ///////////////////////////////////////////////////////////////////////////
// state   48
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},

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
    {                  Token::exp__, {                  TA_PUSH_STATE,   86}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   50
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   87}},

// ///////////////////////////////////////////////////////////////////////////
// state   51
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
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   88}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   52
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   69}},

// ///////////////////////////////////////////////////////////////////////////
// state   53
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   72}},

// ///////////////////////////////////////////////////////////////////////////
// state   54
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   56}},

// ///////////////////////////////////////////////////////////////////////////
// state   55
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   58}},

// ///////////////////////////////////////////////////////////////////////////
// state   56
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   54}},

// ///////////////////////////////////////////////////////////////////////////
// state   57
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   60}},

// ///////////////////////////////////////////////////////////////////////////
// state   58
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   92}},

// ///////////////////////////////////////////////////////////////////////////
// state   59
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   62}},

// ///////////////////////////////////////////////////////////////////////////
// state   60
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   93}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

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
    {                  Token::exp__, {                  TA_PUSH_STATE,   94}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   95}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   96}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   97}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   98}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   99}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  100}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  101}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  102}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  103}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  104}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  105}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  106}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  107}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  108}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  109}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   77
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   57}},

// ///////////////////////////////////////////////////////////////////////////
// state   78
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   59}},

// ///////////////////////////////////////////////////////////////////////////
// state   79
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
// state   80
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  111}},
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
    {           Token::param_list__, {                  TA_PUSH_STATE,  113}},

// ///////////////////////////////////////////////////////////////////////////
// state   81
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   17}},

// ///////////////////////////////////////////////////////////////////////////
// state   82
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   50}},

// ///////////////////////////////////////////////////////////////////////////
// state   83
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   15}},

// ///////////////////////////////////////////////////////////////////////////
// state   84
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  114}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state   85
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   21}},

// ///////////////////////////////////////////////////////////////////////////
// state   86
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  115}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state   87
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  116}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  117}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  118}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  119}},

// ///////////////////////////////////////////////////////////////////////////
// state   88
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
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  120}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   89
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  121}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   90
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  122}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  123}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   92
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  124}},

// ///////////////////////////////////////////////////////////////////////////
// state   93
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state   94
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  125}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state   95
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   34}},

// ///////////////////////////////////////////////////////////////////////////
// state   96
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   33}},

// ///////////////////////////////////////////////////////////////////////////
// state   97
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   35}},

// ///////////////////////////////////////////////////////////////////////////
// state   98
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state   99
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  100
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  101
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  102
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   46}},

// ///////////////////////////////////////////////////////////////////////////
// state  103
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state  104
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  105
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state  106
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   48}},

// ///////////////////////////////////////////////////////////////////////////
// state  107
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   47}},

// ///////////////////////////////////////////////////////////////////////////
// state  108
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  109
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state  110
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   49}},

// ///////////////////////////////////////////////////////////////////////////
// state  111
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   67}},

// ///////////////////////////////////////////////////////////////////////////
// state  112
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   74}},

// ///////////////////////////////////////////////////////////////////////////
// state  113
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  126}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  127}},

// ///////////////////////////////////////////////////////////////////////////
// state  114
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
    {            Token::statement__, {                  TA_PUSH_STATE,  128}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  115
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
    {            Token::statement__, {                  TA_PUSH_STATE,  129}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  116
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  130}},

// ///////////////////////////////////////////////////////////////////////////
// state  117
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  131}},

// ///////////////////////////////////////////////////////////////////////////
// state  118
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  132}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  133}},

// ///////////////////////////////////////////////////////////////////////////
// state  119
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    9}},

// ///////////////////////////////////////////////////////////////////////////
// state  120
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  134}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  135}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state  121
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   70}},

// ///////////////////////////////////////////////////////////////////////////
// state  122
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   73}},

// ///////////////////////////////////////////////////////////////////////////
// state  123
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  136}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state  124
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  137}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  138}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  119}},

// ///////////////////////////////////////////////////////////////////////////
// state  125
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   55}},

// ///////////////////////////////////////////////////////////////////////////
// state  126
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   68}},

// ///////////////////////////////////////////////////////////////////////////
// state  127
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  139}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state  128
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   18}},

// ///////////////////////////////////////////////////////////////////////////
// state  129
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  140}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   20}},

// ///////////////////////////////////////////////////////////////////////////
// state  130
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  141}},

// ///////////////////////////////////////////////////////////////////////////
// state  131
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  142}},

// ///////////////////////////////////////////////////////////////////////////
// state  132
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  143}},

// ///////////////////////////////////////////////////////////////////////////
// state  133
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,  144}},

// ///////////////////////////////////////////////////////////////////////////
// state  134
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
    {            Token::statement__, {                  TA_PUSH_STATE,  145}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  135
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  146}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state  136
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   71}},

// ///////////////////////////////////////////////////////////////////////////
// state  137
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  147}},

// ///////////////////////////////////////////////////////////////////////////
// state  138
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  148}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  133}},

// ///////////////////////////////////////////////////////////////////////////
// state  139
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   64}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   66}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   68}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   72}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   75}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   79}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   75}},

// ///////////////////////////////////////////////////////////////////////////
// state  140
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
    {            Token::statement__, {                  TA_PUSH_STATE,  149}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  141
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  150}},

// ///////////////////////////////////////////////////////////////////////////
// state  142
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  151}},
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
// state  143
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  152}},

// ///////////////////////////////////////////////////////////////////////////
// state  144
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},

// ///////////////////////////////////////////////////////////////////////////
// state  145
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   23}},

// ///////////////////////////////////////////////////////////////////////////
// state  146
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
    {            Token::statement__, {                  TA_PUSH_STATE,  153}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   31}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   32}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   33}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   34}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   35}},
    {                 Token::call__, {                  TA_PUSH_STATE,   36}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  147
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  154}},

// ///////////////////////////////////////////////////////////////////////////
// state  148
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  155}},

// ///////////////////////////////////////////////////////////////////////////
// state  149
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   19}},

// ///////////////////////////////////////////////////////////////////////////
// state  150
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  156}},
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
// state  151
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    4}},

// ///////////////////////////////////////////////////////////////////////////
// state  152
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  157}},
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
// state  153
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   24}},

// ///////////////////////////////////////////////////////////////////////////
// state  154
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  158}},
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
// state  155
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  159}},

// ///////////////////////////////////////////////////////////////////////////
// state  156
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    3}},

// ///////////////////////////////////////////////////////////////////////////
// state  157
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    2}},

// ///////////////////////////////////////////////////////////////////////////
// state  158
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    6}},

// ///////////////////////////////////////////////////////////////////////////
// state  159
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  160}},
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
// state  160
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    5}}

};

unsigned int const SteelParser::ms_state_transition_count =
    sizeof(SteelParser::ms_state_transition) /
    sizeof(SteelParser::StateTransition);


#line 47 "steel.trison"


void SteelParser::addError(unsigned int line, const std::string &error)
{
	mbErrorEncountered = true;
	mErrors +=  itos(line) + ": " + error + '\n';
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

#line 4456 "SteelParser.cpp"

