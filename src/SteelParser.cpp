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

#line 132 "steel.trison"

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

#line 145 "steel.trison"

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

#line 155 "steel.trison"

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

#line 167 "steel.trison"

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

#line 179 "steel.trison"

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

#line 192 "steel.trison"

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

#line 206 "steel.trison"

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

#line 220 "steel.trison"
 return id; 
#line 653 "SteelParser.cpp"
}

// rule 9: param_id <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0009 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 222 "steel.trison"
 return id; 
#line 664 "SteelParser.cpp"
}

// rule 10: param_definition <-     
AstBase* SteelParser::ReductionRuleHandler0010 ()
{

#line 227 "steel.trison"

		 return new AstParamDefinitionList(GET_LINE(), GET_SCRIPT());
	
#line 675 "SteelParser.cpp"
}

// rule 11: param_definition <- vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0011 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 232 "steel.trison"

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

#line 239 "steel.trison"

				list->add(static_cast<AstDeclaration*>(decl));
				return list;
			
#line 706 "SteelParser.cpp"
}

// rule 13: statement_list <-     
AstBase* SteelParser::ReductionRuleHandler0013 ()
{

#line 247 "steel.trison"

				AstStatementList *pList = 
					new AstStatementList(m_scanner->getCurrentLine(),
										m_scanner->getScriptName());
				return pList;
			
#line 720 "SteelParser.cpp"
}

// rule 14: statement_list <- statement_list:list statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0014 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 255 "steel.trison"

					list->add( stmt ); 
					return list;
				
#line 736 "SteelParser.cpp"
}

// rule 15: statement <- %error    
AstBase* SteelParser::ReductionRuleHandler0015 ()
{

#line 263 "steel.trison"
 
			addError(GET_LINE(),"parse error");
			return new AstStatement(GET_LINE(),GET_SCRIPT());
		
#line 748 "SteelParser.cpp"
}

// rule 16: statement <- exp_statement:exp    
AstBase* SteelParser::ReductionRuleHandler0016 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 268 "steel.trison"
 return new AstExpressionStatement(exp->GetLine(),exp->GetScript(), exp); 
#line 759 "SteelParser.cpp"
}

// rule 17: statement <- func_definition:func    
AstBase* SteelParser::ReductionRuleHandler0017 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFunctionDefinition* func = static_cast< AstFunctionDefinition* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 270 "steel.trison"
 return func; 
#line 770 "SteelParser.cpp"
}

// rule 18: statement <- '{' statement_list:list '}'    
AstBase* SteelParser::ReductionRuleHandler0018 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 272 "steel.trison"
 return list; 
#line 781 "SteelParser.cpp"
}

// rule 19: statement <- '{' '}'    
AstBase* SteelParser::ReductionRuleHandler0019 ()
{

#line 274 "steel.trison"

			 return new AstStatement(GET_LINE(),GET_SCRIPT());
			
#line 792 "SteelParser.cpp"
}

// rule 20: statement <- vardecl:vardecl ';'    
AstBase* SteelParser::ReductionRuleHandler0020 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* vardecl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 278 "steel.trison"
 return vardecl; 
#line 803 "SteelParser.cpp"
}

// rule 21: statement <- WHILE '(' exp:exp ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0021 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 280 "steel.trison"
 return new AstWhileStatement(exp->GetLine(), exp->GetScript(),exp,stmt); 
#line 816 "SteelParser.cpp"
}

// rule 22: statement <- WHILE '('    
AstBase* SteelParser::ReductionRuleHandler0022 ()
{

#line 283 "steel.trison"
 
				addError(GET_LINE(),"expected ')'");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT())); 
			
#line 829 "SteelParser.cpp"
}

// rule 23: statement <- WHILE %error    
AstBase* SteelParser::ReductionRuleHandler0023 ()
{

#line 290 "steel.trison"
 
				addError(GET_LINE(),"missing loop condition.");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT())); 
			
#line 842 "SteelParser.cpp"
}

// rule 24: statement <- WHILE '(' %error ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0024 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 298 "steel.trison"
 
				addError(GET_LINE(),"error in loop expression.");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt);    
			
#line 856 "SteelParser.cpp"
}

// rule 25: statement <- IF '(' exp:exp ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0025 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 303 "steel.trison"
 return new AstIfStatement(exp->GetLine(), exp->GetScript(),exp,stmt,elses);
#line 871 "SteelParser.cpp"
}

// rule 26: statement <- IF '(' exp:exp ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0026 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 305 "steel.trison"
 return new AstIfStatement(exp->GetLine(),exp->GetScript(),exp,stmt); 
#line 884 "SteelParser.cpp"
}

// rule 27: statement <- IF '(' %error ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0027 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 308 "steel.trison"

			addError(GET_LINE(),"parse error in if condition."); 
			return new AstIfStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt,elses);
		
#line 900 "SteelParser.cpp"
}

// rule 28: statement <- IF '(' %error ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0028 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 314 "steel.trison"

				addError(GET_LINE(),"parse error in if condition."); 
				return new AstIfStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt);
			
#line 914 "SteelParser.cpp"
}

// rule 29: statement <- RETURN exp:exp ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0029 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 320 "steel.trison"
 return new AstReturnStatement(exp->GetLine(),exp->GetScript(),exp);
#line 925 "SteelParser.cpp"
}

// rule 30: statement <- RETURN ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0030 ()
{

#line 323 "steel.trison"

				return new AstReturnStatement(GET_LINE(),GET_SCRIPT());
			
#line 936 "SteelParser.cpp"
}

// rule 31: statement <- RETURN %error    
AstBase* SteelParser::ReductionRuleHandler0031 ()
{

#line 328 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'return'");
				return new AstReturnStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 948 "SteelParser.cpp"
}

// rule 32: statement <- RETURN    
AstBase* SteelParser::ReductionRuleHandler0032 ()
{

#line 334 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'return'");
				return new AstReturnStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 960 "SteelParser.cpp"
}

// rule 33: statement <- FOR '(' exp_statement:start exp_statement:condition ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0033 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(5 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);

#line 343 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition, 
						new AstExpression (start->GetLine(),start->GetScript()), stmt); 
			
#line 978 "SteelParser.cpp"
}

// rule 34: statement <- FOR '(' exp_statement:start exp_statement:condition exp:iteration ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0034 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* iteration = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 349 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition,iteration,stmt);
			
#line 997 "SteelParser.cpp"
}

// rule 35: statement <- FOR '(' %error    
AstBase* SteelParser::ReductionRuleHandler0035 ()
{

#line 354 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1011 "SteelParser.cpp"
}

// rule 36: statement <- FOR '(' exp_statement:start %error    
AstBase* SteelParser::ReductionRuleHandler0036 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 362 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1027 "SteelParser.cpp"
}

// rule 37: statement <- FOR '(' exp_statement:start exp_statement:condition %error    
AstBase* SteelParser::ReductionRuleHandler0037 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 370 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1045 "SteelParser.cpp"
}

// rule 38: statement <- FOR '(' exp_statement:start exp_statement:condition exp:iteration    
AstBase* SteelParser::ReductionRuleHandler0038 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* iteration = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 378 "steel.trison"

				addError(GET_LINE(),"malformed for loop. Expected ')'");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1065 "SteelParser.cpp"
}

// rule 39: statement <- FOR %error    
AstBase* SteelParser::ReductionRuleHandler0039 ()
{

#line 386 "steel.trison"

				addError(GET_LINE(),"malformed for loop. Expected '('");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1079 "SteelParser.cpp"
}

// rule 40: statement <- BREAK ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0040 ()
{

#line 395 "steel.trison"

				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1090 "SteelParser.cpp"
}

// rule 41: statement <- BREAK %error    
AstBase* SteelParser::ReductionRuleHandler0041 ()
{

#line 400 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'break'");
				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1102 "SteelParser.cpp"
}

// rule 42: statement <- BREAK    
AstBase* SteelParser::ReductionRuleHandler0042 ()
{

#line 406 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'break'");
				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1114 "SteelParser.cpp"
}

// rule 43: statement <- CONTINUE ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0043 ()
{

#line 414 "steel.trison"

				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1125 "SteelParser.cpp"
}

// rule 44: statement <- CONTINUE %error    
AstBase* SteelParser::ReductionRuleHandler0044 ()
{

#line 419 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'continue'");
				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1137 "SteelParser.cpp"
}

// rule 45: statement <- CONTINUE    
AstBase* SteelParser::ReductionRuleHandler0045 ()
{

#line 425 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'continue'");
				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1149 "SteelParser.cpp"
}

// rule 46: exp <- call:call    
AstBase* SteelParser::ReductionRuleHandler0046 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstCallExpression* call = static_cast< AstCallExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 436 "steel.trison"
 return call; 
#line 1160 "SteelParser.cpp"
}

// rule 47: exp <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0047 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 438 "steel.trison"
 return i;
#line 1171 "SteelParser.cpp"
}

// rule 48: exp <- FLOAT:f    
AstBase* SteelParser::ReductionRuleHandler0048 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* f = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 440 "steel.trison"
 return f; 
#line 1182 "SteelParser.cpp"
}

// rule 49: exp <- STRING:s    
AstBase* SteelParser::ReductionRuleHandler0049 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* s = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 442 "steel.trison"
 return s; 
#line 1193 "SteelParser.cpp"
}

// rule 50: exp <- var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0050 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 444 "steel.trison"
 return id; 
#line 1204 "SteelParser.cpp"
}

// rule 51: exp <- array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0051 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 446 "steel.trison"
 return id; 
#line 1215 "SteelParser.cpp"
}

// rule 52: exp <- exp:a '+' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0052 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 448 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,b); 
#line 1228 "SteelParser.cpp"
}

// rule 53: exp <- exp:a '+'     %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0053 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 450 "steel.trison"
 
				addError(a->GetLine(),"expected expression before ';'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1242 "SteelParser.cpp"
}

// rule 54: exp <- exp:a '-' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0054 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 455 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,b); 
#line 1255 "SteelParser.cpp"
}

// rule 55: exp <- exp:a '*' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0055 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 457 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,b); 
#line 1268 "SteelParser.cpp"
}

// rule 56: exp <- exp:a '/' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0056 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 459 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::DIV,a,b); 
#line 1281 "SteelParser.cpp"
}

// rule 57: exp <- exp:a '%' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0057 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 461 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MOD,a,b); 
#line 1294 "SteelParser.cpp"
}

// rule 58: exp <- exp:a D exp:b    %left %prec POW
AstBase* SteelParser::ReductionRuleHandler0058 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 463 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::D,a,b); 
#line 1307 "SteelParser.cpp"
}

// rule 59: exp <- exp:lvalue '=' exp:exp    %right %prec ASSIGNMENT
AstBase* SteelParser::ReductionRuleHandler0059 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 465 "steel.trison"
 return new AstVarAssignmentExpression(lvalue->GetLine(),lvalue->GetScript(),lvalue,exp); 
#line 1320 "SteelParser.cpp"
}

// rule 60: exp <- exp:a '^' exp:b    %right %prec POW
AstBase* SteelParser::ReductionRuleHandler0060 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 467 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::POW,a,b); 
#line 1333 "SteelParser.cpp"
}

// rule 61: exp <- exp:a OR exp:b    %left %prec OR
AstBase* SteelParser::ReductionRuleHandler0061 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 469 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::OR,a,b); 
#line 1346 "SteelParser.cpp"
}

// rule 62: exp <- exp:a AND exp:b    %left %prec AND
AstBase* SteelParser::ReductionRuleHandler0062 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 471 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::AND,a,b); 
#line 1359 "SteelParser.cpp"
}

// rule 63: exp <- exp:a EQ exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0063 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 473 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::EQ,a,b); 
#line 1372 "SteelParser.cpp"
}

// rule 64: exp <- exp:a NE exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0064 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 475 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::NE,a,b); 
#line 1385 "SteelParser.cpp"
}

// rule 65: exp <- exp:a LT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0065 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 477 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LT,a,b); 
#line 1398 "SteelParser.cpp"
}

// rule 66: exp <- exp:a GT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0066 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 479 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GT,a,b); 
#line 1411 "SteelParser.cpp"
}

// rule 67: exp <- exp:a LTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0067 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 481 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LTE,a,b); 
#line 1424 "SteelParser.cpp"
}

// rule 68: exp <- exp:a GTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0068 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 483 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GTE,a,b); 
#line 1437 "SteelParser.cpp"
}

// rule 69: exp <- exp:a CAT exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0069 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 485 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::CAT,a,b); 
#line 1450 "SteelParser.cpp"
}

// rule 70: exp <- '(' exp:exp ')'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0070 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 487 "steel.trison"
 return exp; 
#line 1461 "SteelParser.cpp"
}

// rule 71: exp <- '-' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0071 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 489 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::MINUS,exp); 
#line 1472 "SteelParser.cpp"
}

// rule 72: exp <- '-' %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0072 ()
{

#line 491 "steel.trison"

						addError(GET_LINE(),"expected expression after unary minus.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::MINUS,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1484 "SteelParser.cpp"
}

// rule 73: exp <- '+' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0073 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 496 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::PLUS,exp); 
#line 1495 "SteelParser.cpp"
}

// rule 74: exp <- '+' %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0074 ()
{

#line 498 "steel.trison"

						addError(GET_LINE(),"expected expression after unary plus.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::PLUS,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1507 "SteelParser.cpp"
}

// rule 75: exp <- NOT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0075 ()
{

#line 503 "steel.trison"

						addError(GET_LINE(),"expected expression after unary minus.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::NOT,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1519 "SteelParser.cpp"
}

// rule 76: exp <- CAT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0076 ()
{

#line 511 "steel.trison"

						addError(GET_LINE(),"expected expression after ':'.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::CAT,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1531 "SteelParser.cpp"
}

// rule 77: exp <- NOT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0077 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 517 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::NOT,exp); 
#line 1542 "SteelParser.cpp"
}

// rule 78: exp <- CAT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0078 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 519 "steel.trison"
 return new AstUnaryOp(exp->GetLine(),
exp->GetScript(), AstUnaryOp::CAT,exp); 
#line 1554 "SteelParser.cpp"
}

// rule 79: exp <- exp:lvalue '[' exp:index ']'    
AstBase* SteelParser::ReductionRuleHandler0079 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* index = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 522 "steel.trison"
 return new AstArrayElement(lvalue->GetLine(),lvalue->GetScript(),lvalue,index); 
#line 1567 "SteelParser.cpp"
}

// rule 80: exp <- exp:lvalue '[' ']'    
AstBase* SteelParser::ReductionRuleHandler0080 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 525 "steel.trison"
 
								addError(GET_LINE(),"expected expression in array index");
								return new AstArrayElement(lvalue->GetLine(),lvalue->GetScript(),lvalue, new AstExpression(GET_LINE(),GET_SCRIPT()));
						
#line 1581 "SteelParser.cpp"
}

// rule 81: exp <- INCREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0081 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 530 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::PRE);
#line 1592 "SteelParser.cpp"
}

// rule 82: exp <- INCREMENT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0082 ()
{

#line 532 "steel.trison"

										addError(GET_LINE(),"expected lvalue after '++'");
										return new AstIncrement(GET_LINE(),GET_SCRIPT(),
												new AstExpression(GET_LINE(),GET_SCRIPT()),AstIncrement::PRE);
										
#line 1605 "SteelParser.cpp"
}

// rule 83: exp <- %error INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0083 ()
{

#line 538 "steel.trison"
 
									addError(GET_LINE(),"expected lvalue before '++'");
									return new AstIncrement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()), AstIncrement::POST);
	   							
#line 1617 "SteelParser.cpp"
}

// rule 84: exp <- exp:lvalue INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0084 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 544 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::POST);
#line 1628 "SteelParser.cpp"
}

// rule 85: exp <- DECREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0085 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 546 "steel.trison"
 return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::PRE);
#line 1639 "SteelParser.cpp"
}

// rule 86: exp <- DECREMENT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0086 ()
{

#line 548 "steel.trison"

										addError(GET_LINE(),"expected lvalue after '--'");
										return new AstDecrement(GET_LINE(),GET_SCRIPT(),
												new AstExpression(GET_LINE(),GET_SCRIPT()),AstDecrement::PRE);
										
#line 1652 "SteelParser.cpp"
}

// rule 87: exp <- exp:lvalue DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0087 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 555 "steel.trison"
 
									return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::POST);
									
#line 1665 "SteelParser.cpp"
}

// rule 88: exp <- %error DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0088 ()
{

#line 559 "steel.trison"
 
									addError(GET_LINE(),"expected lvalue before '--'");
									return new AstDecrement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()), AstDecrement::POST);
									
#line 1677 "SteelParser.cpp"
}

// rule 89: exp <- POP exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0089 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 565 "steel.trison"
 return new AstPop(lvalue->GetLine(),lvalue->GetScript(),lvalue); 
#line 1688 "SteelParser.cpp"
}

// rule 90: exp <- POP %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0090 ()
{

#line 567 "steel.trison"

						addError(GET_LINE(),"expected expression after 'pop'.");
						return new AstPop(GET_LINE(),GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()));
							  
#line 1700 "SteelParser.cpp"
}

// rule 91: exp_statement <- ';'    
AstBase* SteelParser::ReductionRuleHandler0091 ()
{

#line 576 "steel.trison"

			return new AstExpression(GET_LINE(),GET_SCRIPT()); 
		
#line 1711 "SteelParser.cpp"
}

// rule 92: exp_statement <- exp:exp ';'    
AstBase* SteelParser::ReductionRuleHandler0092 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 580 "steel.trison"
 return exp; 
#line 1722 "SteelParser.cpp"
}

// rule 93: int_literal <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0093 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* i = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 585 "steel.trison"
 return i; 
#line 1733 "SteelParser.cpp"
}

// rule 94: var_identifier <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0094 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 590 "steel.trison"
 return id; 
#line 1744 "SteelParser.cpp"
}

// rule 95: func_identifier <- FUNC_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0095 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 595 "steel.trison"
 return id; 
#line 1755 "SteelParser.cpp"
}

// rule 96: array_identifier <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0096 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 600 "steel.trison"
 return id; 
#line 1766 "SteelParser.cpp"
}

// rule 97: call <- func_identifier:id '(' ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0097 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 606 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id);
#line 1777 "SteelParser.cpp"
}

// rule 98: call <- func_identifier:id '(' param_list:params ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0098 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 608 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id,params);
#line 1790 "SteelParser.cpp"
}

// rule 99: call <- func_identifier:id '(' param_list:params    
AstBase* SteelParser::ReductionRuleHandler0099 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 611 "steel.trison"

				addError(GET_LINE(),"expected ')' before ';'.");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id,params); 
			
#line 1806 "SteelParser.cpp"
}

// rule 100: call <- func_identifier:id '('    
AstBase* SteelParser::ReductionRuleHandler0100 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 617 "steel.trison"

				addError(GET_LINE(),"expected ')' before ';'");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1820 "SteelParser.cpp"
}

// rule 101: call <- func_identifier:id %error ')'    
AstBase* SteelParser::ReductionRuleHandler0101 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 623 "steel.trison"

				addError(GET_LINE(),"missing '(' in function call");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1834 "SteelParser.cpp"
}

// rule 102: call <- func_identifier:id %error    
AstBase* SteelParser::ReductionRuleHandler0102 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 629 "steel.trison"

				addError(GET_LINE(),"function call missing parentheses.");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1848 "SteelParser.cpp"
}

// rule 103: call <- func_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0103 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 635 "steel.trison"

				addError(GET_LINE(),"invalid bareword. function call missing parentheses?");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1862 "SteelParser.cpp"
}

// rule 104: vardecl <- VAR var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0104 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 644 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id);
#line 1873 "SteelParser.cpp"
}

// rule 105: vardecl <- VAR var_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0105 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 646 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id,exp); 
#line 1886 "SteelParser.cpp"
}

// rule 106: vardecl <- VAR array_identifier:id '[' exp:i ']'    
AstBase* SteelParser::ReductionRuleHandler0106 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* i = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 648 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id,i); 
#line 1899 "SteelParser.cpp"
}

// rule 107: vardecl <- VAR array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0107 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 650 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id); 
#line 1910 "SteelParser.cpp"
}

// rule 108: vardecl <- VAR array_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0108 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 652 "steel.trison"

							AstArrayDeclaration *pDecl =  new AstArrayDeclaration(id->GetLine(),id->GetScript(),id);
							pDecl->assign(exp);
							return pDecl;
						 
#line 1927 "SteelParser.cpp"
}

// rule 109: param_list <- exp:exp    
AstBase* SteelParser::ReductionRuleHandler0109 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 661 "steel.trison"
 AstParamList * pList = new AstParamList ( exp->GetLine(), exp->GetScript() );
		  pList->add(exp);
		  return pList;
		
#line 1941 "SteelParser.cpp"
}

// rule 110: param_list <- param_list:list ',' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0110 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamList* list = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 666 "steel.trison"
 list->add(exp); return list;
#line 1954 "SteelParser.cpp"
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
    {       Token::statement_list__,  0, &SteelParser::ReductionRuleHandler0013, "rule 13: statement_list <-     "},
    {       Token::statement_list__,  2, &SteelParser::ReductionRuleHandler0014, "rule 14: statement_list <- statement_list statement    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0015, "rule 15: statement <- %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0016, "rule 16: statement <- exp_statement    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0017, "rule 17: statement <- func_definition    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0018, "rule 18: statement <- '{' statement_list '}'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0019, "rule 19: statement <- '{' '}'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0020, "rule 20: statement <- vardecl ';'    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0021, "rule 21: statement <- WHILE '(' exp ')' statement     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0022, "rule 22: statement <- WHILE '('    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0023, "rule 23: statement <- WHILE %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0024, "rule 24: statement <- WHILE '(' %error ')' statement    "},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0025, "rule 25: statement <- IF '(' exp ')' statement ELSE statement     %prec ELSE"},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0026, "rule 26: statement <- IF '(' exp ')' statement     %prec NON_ELSE"},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0027, "rule 27: statement <- IF '(' %error ')' statement ELSE statement     %prec ELSE"},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0028, "rule 28: statement <- IF '(' %error ')' statement     %prec NON_ELSE"},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0029, "rule 29: statement <- RETURN exp ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0030, "rule 30: statement <- RETURN ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0031, "rule 31: statement <- RETURN %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0032, "rule 32: statement <- RETURN    "},
    {            Token::statement__,  6, &SteelParser::ReductionRuleHandler0033, "rule 33: statement <- FOR '(' exp_statement exp_statement ')' statement     %prec CORRECT"},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0034, "rule 34: statement <- FOR '(' exp_statement exp_statement exp ')' statement     %prec CORRECT"},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0035, "rule 35: statement <- FOR '(' %error    "},
    {            Token::statement__,  4, &SteelParser::ReductionRuleHandler0036, "rule 36: statement <- FOR '(' exp_statement %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0037, "rule 37: statement <- FOR '(' exp_statement exp_statement %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0038, "rule 38: statement <- FOR '(' exp_statement exp_statement exp    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0039, "rule 39: statement <- FOR %error    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0040, "rule 40: statement <- BREAK ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0041, "rule 41: statement <- BREAK %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0042, "rule 42: statement <- BREAK    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0043, "rule 43: statement <- CONTINUE ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0044, "rule 44: statement <- CONTINUE %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0045, "rule 45: statement <- CONTINUE    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0046, "rule 46: exp <- call    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0047, "rule 47: exp <- INT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0048, "rule 48: exp <- FLOAT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0049, "rule 49: exp <- STRING    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0050, "rule 50: exp <- var_identifier    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0051, "rule 51: exp <- array_identifier    "},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0052, "rule 52: exp <- exp '+' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0053, "rule 53: exp <- exp '+'     %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0054, "rule 54: exp <- exp '-' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0055, "rule 55: exp <- exp '*' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0056, "rule 56: exp <- exp '/' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0057, "rule 57: exp <- exp '%' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0058, "rule 58: exp <- exp D exp    %left %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0059, "rule 59: exp <- exp '=' exp    %right %prec ASSIGNMENT"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0060, "rule 60: exp <- exp '^' exp    %right %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0061, "rule 61: exp <- exp OR exp    %left %prec OR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0062, "rule 62: exp <- exp AND exp    %left %prec AND"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0063, "rule 63: exp <- exp EQ exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0064, "rule 64: exp <- exp NE exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0065, "rule 65: exp <- exp LT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0066, "rule 66: exp <- exp GT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0067, "rule 67: exp <- exp LTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0068, "rule 68: exp <- exp GTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0069, "rule 69: exp <- exp CAT exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0070, "rule 70: exp <- '(' exp ')'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0071, "rule 71: exp <- '-' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0072, "rule 72: exp <- '-' %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0073, "rule 73: exp <- '+' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0074, "rule 74: exp <- '+' %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0075, "rule 75: exp <- NOT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0076, "rule 76: exp <- CAT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0077, "rule 77: exp <- NOT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0078, "rule 78: exp <- CAT exp     %prec UNARY"},
    {                  Token::exp__,  4, &SteelParser::ReductionRuleHandler0079, "rule 79: exp <- exp '[' exp ']'    "},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0080, "rule 80: exp <- exp '[' ']'    "},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0081, "rule 81: exp <- INCREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0082, "rule 82: exp <- INCREMENT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0083, "rule 83: exp <- %error INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0084, "rule 84: exp <- exp INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0085, "rule 85: exp <- DECREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0086, "rule 86: exp <- DECREMENT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0087, "rule 87: exp <- exp DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0088, "rule 88: exp <- %error DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0089, "rule 89: exp <- POP exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0090, "rule 90: exp <- POP %error     %prec UNARY"},
    {        Token::exp_statement__,  1, &SteelParser::ReductionRuleHandler0091, "rule 91: exp_statement <- ';'    "},
    {        Token::exp_statement__,  2, &SteelParser::ReductionRuleHandler0092, "rule 92: exp_statement <- exp ';'    "},
    {          Token::int_literal__,  1, &SteelParser::ReductionRuleHandler0093, "rule 93: int_literal <- INT    "},
    {       Token::var_identifier__,  1, &SteelParser::ReductionRuleHandler0094, "rule 94: var_identifier <- VAR_IDENTIFIER    "},
    {      Token::func_identifier__,  1, &SteelParser::ReductionRuleHandler0095, "rule 95: func_identifier <- FUNC_IDENTIFIER    "},
    {     Token::array_identifier__,  1, &SteelParser::ReductionRuleHandler0096, "rule 96: array_identifier <- ARRAY_IDENTIFIER    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0097, "rule 97: call <- func_identifier '(' ')'     %prec CORRECT"},
    {                 Token::call__,  4, &SteelParser::ReductionRuleHandler0098, "rule 98: call <- func_identifier '(' param_list ')'     %prec CORRECT"},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0099, "rule 99: call <- func_identifier '(' param_list    "},
    {                 Token::call__,  2, &SteelParser::ReductionRuleHandler0100, "rule 100: call <- func_identifier '('    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0101, "rule 101: call <- func_identifier %error ')'    "},
    {                 Token::call__,  2, &SteelParser::ReductionRuleHandler0102, "rule 102: call <- func_identifier %error    "},
    {                 Token::call__,  1, &SteelParser::ReductionRuleHandler0103, "rule 103: call <- func_identifier    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0104, "rule 104: vardecl <- VAR var_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0105, "rule 105: vardecl <- VAR var_identifier '=' exp    "},
    {              Token::vardecl__,  5, &SteelParser::ReductionRuleHandler0106, "rule 106: vardecl <- VAR array_identifier '[' exp ']'    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0107, "rule 107: vardecl <- VAR array_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0108, "rule 108: vardecl <- VAR array_identifier '=' exp    "},
    {           Token::param_list__,  1, &SteelParser::ReductionRuleHandler0109, "rule 109: param_list <- exp    "},
    {           Token::param_list__,  3, &SteelParser::ReductionRuleHandler0110, "rule 110: param_list <- param_list ',' exp    "},

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
    {   5,   27,    0,   32,    9}, // state    2
    {   0,    0,   41,    0,    0}, // state    3
    {  42,    2,   44,    0,    0}, // state    4
    {   0,    0,   45,    0,    0}, // state    5
    {  46,   15,    0,   61,    5}, // state    6
    {  66,    1,   67,   68,    1}, // state    7
    {  69,   15,    0,   84,    5}, // state    8
    {  89,   15,    0,  104,    5}, // state    9
    { 109,   15,    0,  124,    5}, // state   10
    { 129,    2,    0,    0,    0}, // state   11
    { 131,   29,    0,    0,    0}, // state   12
    { 160,   29,    0,    0,    0}, // state   13
    { 189,   29,    0,  218,    5}, // state   14
    { 223,    1,    0,    0,    0}, // state   15
    { 224,    2,    0,    0,    0}, // state   16
    {   0,    0,  226,    0,    0}, // state   17
    {   0,    0,  227,    0,    0}, // state   18
    {   0,    0,  228,    0,    0}, // state   19
    { 229,    2,    0,    0,    0}, // state   20
    { 231,    2,    0,  233,    2}, // state   21
    {   0,    0,  235,    0,    0}, // state   22
    {   0,    0,  236,    0,    0}, // state   23
    {   0,    0,  237,    0,    0}, // state   24
    { 238,   15,    0,  253,    5}, // state   25
    { 258,   15,    0,  273,    5}, // state   26
    { 278,   15,    0,  293,    5}, // state   27
    { 298,   15,    0,  313,    5}, // state   28
    { 318,    1,    0,    0,    0}, // state   29
    {   0,    0,  319,    0,    0}, // state   30
    {   0,    0,  320,    0,    0}, // state   31
    { 321,   21,    0,    0,    0}, // state   32
    {   0,    0,  342,    0,    0}, // state   33
    {   0,    0,  343,    0,    0}, // state   34
    { 344,   47,    0,    0,    0}, // state   35
    {   0,    0,  391,    0,    0}, // state   36
    {   0,    0,  392,    0,    0}, // state   37
    { 393,    1,    0,    0,    0}, // state   38
    {   0,    0,  394,    0,    0}, // state   39
    {   0,    0,  395,    0,    0}, // state   40
    { 396,    2,    0,    0,    0}, // state   41
    { 398,   21,    0,    0,    0}, // state   42
    {   0,    0,  419,    0,    0}, // state   43
    { 420,   27,    0,  447,    9}, // state   44
    { 456,    2,  458,    0,    0}, // state   45
    { 459,    4,  463,    0,    0}, // state   46
    { 464,    2,  466,    0,    0}, // state   47
    { 467,    4,  471,    0,    0}, // state   48
    { 472,    2,  474,    0,    0}, // state   49
    { 475,    4,  479,    0,    0}, // state   50
    {   0,    0,  480,    0,    0}, // state   51
    { 481,   29,    0,  510,    5}, // state   52
    {   0,    0,  515,    0,    0}, // state   53
    {   0,    0,  516,    0,    0}, // state   54
    {   0,    0,  517,    0,    0}, // state   55
    {   0,    0,  518,    0,    0}, // state   56
    { 519,    2,  521,    0,    0}, // state   57
    {   0,    0,  522,    0,    0}, // state   58
    { 523,   21,    0,    0,    0}, // state   59
    { 544,   15,    0,  559,    5}, // state   60
    { 564,    1,    0,    0,    0}, // state   61
    { 565,    1,    0,    0,    0}, // state   62
    {   0,    0,  566,    0,    0}, // state   63
    { 567,   16,    0,  583,    6}, // state   64
    { 589,    1,  590,    0,    0}, // state   65
    { 591,    2,  593,    0,    0}, // state   66
    { 594,    2,  596,    0,    0}, // state   67
    { 597,    4,  601,    0,    0}, // state   68
    { 602,    2,  604,    0,    0}, // state   69
    { 605,    4,  609,    0,    0}, // state   70
    { 610,    2,  612,    0,    0}, // state   71
    { 613,    4,  617,    0,    0}, // state   72
    { 618,    2,  620,    0,    0}, // state   73
    { 621,    4,  625,    0,    0}, // state   74
    { 626,    2,    0,    0,    0}, // state   75
    {   0,    0,  628,    0,    0}, // state   76
    { 629,   15,    0,  644,    5}, // state   77
    { 649,   16,    0,  665,    5}, // state   78
    { 670,   15,    0,  685,    5}, // state   79
    { 690,   47,    0,  737,    5}, // state   80
    { 742,   15,    0,  757,    5}, // state   81
    { 762,   15,    0,  777,    5}, // state   82
    { 782,   15,    0,  797,    5}, // state   83
    { 802,   15,    0,  817,    5}, // state   84
    { 822,   15,    0,  837,    5}, // state   85
    { 842,   15,    0,  857,    5}, // state   86
    { 862,   15,    0,  877,    5}, // state   87
    { 882,   15,    0,  897,    5}, // state   88
    { 902,   15,    0,  917,    5}, // state   89
    { 922,   15,    0,  937,    5}, // state   90
    { 942,   15,    0,  957,    5}, // state   91
    { 962,   15,    0,  977,    5}, // state   92
    { 982,   15,    0,  997,    5}, // state   93
    {   0,    0, 1002,    0,    0}, // state   94
    {   0,    0, 1003,    0,    0}, // state   95
    {1004,   15,    0, 1019,    5}, // state   96
    {1024,    1, 1025,    0,    0}, // state   97
    {1026,   47,    0, 1073,    6}, // state   98
    {   0,    0, 1079,    0,    0}, // state   99
    {   0,    0, 1080,    0,    0}, // state  100
    {   0,    0, 1081,    0,    0}, // state  101
    {1082,    3,    0,    0,    0}, // state  102
    {1085,   21,    0,    0,    0}, // state  103
    {   0,    0, 1106,    0,    0}, // state  104
    {1107,    3,    0,    0,    0}, // state  105
    {1110,   21,    0,    0,    0}, // state  106
    {1131,    1, 1132, 1133,    2}, // state  107
    {1135,    4,    0, 1139,    2}, // state  108
    {1141,    2, 1143,    0,    0}, // state  109
    {1144,   16,    0, 1160,    6}, // state  110
    {1166,   15,    0, 1181,    5}, // state  111
    {1186,   15,    0, 1201,    5}, // state  112
    {1206,   15,    0, 1221,    5}, // state  113
    {1226,    1,    0,    0,    0}, // state  114
    {1227,    1,    0,    0,    0}, // state  115
    {1228,   19, 1247,    0,    0}, // state  116
    {   0,    0, 1248,    0,    0}, // state  117
    {1249,   21,    0,    0,    0}, // state  118
    {1270,    7, 1277,    0,    0}, // state  119
    {1278,    7, 1285,    0,    0}, // state  120
    {1286,    4, 1290,    0,    0}, // state  121
    {1291,    4, 1295,    0,    0}, // state  122
    {1296,    3, 1299,    0,    0}, // state  123
    {1300,    4, 1304,    0,    0}, // state  124
    {1305,    3, 1308,    0,    0}, // state  125
    {1309,   10, 1319,    0,    0}, // state  126
    {1320,   10, 1330,    0,    0}, // state  127
    {1331,   14, 1345,    0,    0}, // state  128
    {1346,   14, 1360,    0,    0}, // state  129
    {1361,   10, 1371,    0,    0}, // state  130
    {1372,   10, 1382,    0,    0}, // state  131
    {1383,   16, 1399,    0,    0}, // state  132
    {1400,   17, 1417,    0,    0}, // state  133
    {1418,    7, 1425,    0,    0}, // state  134
    {   0,    0, 1426,    0,    0}, // state  135
    {   0,    0, 1427,    0,    0}, // state  136
    {1428,   20, 1448,    0,    0}, // state  137
    {1449,    2, 1451,    0,    0}, // state  138
    {1452,   26,    0, 1478,    9}, // state  139
    {1487,   26,    0, 1513,    9}, // state  140
    {1522,   26,    0, 1548,    9}, // state  141
    {1557,   26,    0, 1583,    9}, // state  142
    {1592,    2,    0,    0,    0}, // state  143
    {   0,    0, 1594,    0,    0}, // state  144
    {1595,    1,    0,    0,    0}, // state  145
    {1596,    2,    0,    0,    0}, // state  146
    {1598,    2, 1600,    0,    0}, // state  147
    {1601,   16,    0, 1617,    5}, // state  148
    {1622,   20, 1642,    0,    0}, // state  149
    {1643,   20, 1663,    0,    0}, // state  150
    {1664,   21,    0,    0,    0}, // state  151
    {1685,    1, 1686, 1687,    2}, // state  152
    {1689,    4,    0, 1693,    2}, // state  153
    {   0,    0, 1695,    0,    0}, // state  154
    {   0,    0, 1696,    0,    0}, // state  155
    {1697,   15,    0, 1712,    5}, // state  156
    {   0,    0, 1717,    0,    0}, // state  157
    {   0,    0, 1718,    0,    0}, // state  158
    {1719,    1, 1720,    0,    0}, // state  159
    {1721,    1, 1722,    0,    0}, // state  160
    {1723,    1,    0,    0,    0}, // state  161
    {1724,    1,    0, 1725,    1}, // state  162
    {1726,    1,    0,    0,    0}, // state  163
    {1727,    1,    0,    0,    0}, // state  164
    {1728,    2, 1730,    0,    0}, // state  165
    {1731,   26,    0, 1757,    9}, // state  166
    {1766,   21, 1787,    0,    0}, // state  167
    {   0,    0, 1788,    0,    0}, // state  168
    {1789,    2,    0,    0,    0}, // state  169
    {1791,    1,    0,    0,    0}, // state  170
    {1792,    2,    0,    0,    0}, // state  171
    {1794,   20, 1814,    0,    0}, // state  172
    {1815,   26,    0, 1841,    9}, // state  173
    {1850,   26,    0, 1876,    9}, // state  174
    {   0,    0, 1885, 1886,    1}, // state  175
    {   0,    0, 1887,    0,    0}, // state  176
    {   0,    0, 1888, 1889,    1}, // state  177
    {   0,    0, 1890, 1891,    1}, // state  178
    {   0,    0, 1892,    0,    0}, // state  179
    {1893,   26,    0, 1919,    9}, // state  180
    {1928,    1,    0,    0,    0}, // state  181
    {1929,    1,    0,    0,    0}, // state  182
    {1930,    1,    0,    0,    0}, // state  183
    {   0,    0, 1931,    0,    0}, // state  184
    {   0,    0, 1932,    0,    0}, // state  185
    {1933,   27,    0, 1960,    9}, // state  186
    {1969,   27,    0, 1996,    9}, // state  187
    {2005,   27,    0, 2032,    9}, // state  188
    {   0,    0, 2041,    0,    0}, // state  189
    {   0,    0, 2042, 2043,    1}, // state  190
    {   0,    0, 2044, 2045,    1}, // state  191
    {   0,    0, 2046, 2047,    1}, // state  192
    {   0,    0, 2048,    0,    0}, // state  193
    {   0,    0, 2049,    0,    0}, // state  194
    {   0,    0, 2050,    0,    0}, // state  195
    {2051,   27,    0, 2078,    9}, // state  196
    {2087,   27,    0, 2114,    9}, // state  197
    {2123,   27,    0, 2150,    9}, // state  198
    {   0,    0, 2159,    0,    0}, // state  199
    {   0,    0, 2160,    0,    0}, // state  200
    {   0,    0, 2161,    0,    0}  // state  201

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
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},
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
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,   31}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state    3
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {TA_REDUCE_AND_ACCEPT_USING_RULE,    0}},

// ///////////////////////////////////////////////////////////////////////////
// state    4
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   15}},

// ///////////////////////////////////////////////////////////////////////////
// state    5
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   91}},

// ///////////////////////////////////////////////////////////////////////////
// state    6
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   42}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state    7
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,   43}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state    8
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   46}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state    9
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   48}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   10
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   49}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   50}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   11
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   51}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   52}},

// ///////////////////////////////////////////////////////////////////////////
// state   12
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   42}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   53}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   54}},
    {              Token::Type('('), {           TA_REDUCE_USING_RULE,   42}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   42}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   42}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,   42}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,   42}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,   42}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   42}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   42}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   42}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   42}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   42}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   42}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   42}},
    {        Token::FUNC_IDENTIFIER, {           TA_REDUCE_USING_RULE,   42}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,   42}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,   42}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   42}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   42}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,   42}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,   42}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,   42}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,   42}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,   42}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,   42}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,   42}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   13
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   45}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   55}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   56}},
    {              Token::Type('('), {           TA_REDUCE_USING_RULE,   45}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   45}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   45}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,   45}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,   45}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,   45}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   45}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   45}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   45}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   45}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   45}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   45}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   45}},
    {        Token::FUNC_IDENTIFIER, {           TA_REDUCE_USING_RULE,   45}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,   45}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,   45}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   45}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   45}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,   45}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,   45}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,   45}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,   45}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,   45}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,   45}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,   45}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state   14
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   32}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   57}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   58}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   32}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   32}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   32}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   32}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   32}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   32}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   32}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   32}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   32}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   32}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   32}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   32}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   59}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   15
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   60}},

// ///////////////////////////////////////////////////////////////////////////
// state   16
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   62}},

// ///////////////////////////////////////////////////////////////////////////
// state   17
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   95}},

// ///////////////////////////////////////////////////////////////////////////
// state   18
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   94}},

// ///////////////////////////////////////////////////////////////////////////
// state   19
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state   20
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   64}},

// ///////////////////////////////////////////////////////////////////////////
// state   21
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    // nonterminal transitions
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   65}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   66}},

// ///////////////////////////////////////////////////////////////////////////
// state   22
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   47}},

// ///////////////////////////////////////////////////////////////////////////
// state   23
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   48}},

// ///////////////////////////////////////////////////////////////////////////
// state   24
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   49}},

// ///////////////////////////////////////////////////////////////////////////
// state   25
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   68}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   26
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   70}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   27
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   71}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   72}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   28
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   73}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   74}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   29
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   75}},

// ///////////////////////////////////////////////////////////////////////////
// state   30
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   17}},

// ///////////////////////////////////////////////////////////////////////////
// state   31
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},

// ///////////////////////////////////////////////////////////////////////////
// state   32
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state   33
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   16}},

// ///////////////////////////////////////////////////////////////////////////
// state   34
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   50}},

// ///////////////////////////////////////////////////////////////////////////
// state   35
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,  103}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,  103}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,  103}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,  103}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,  103}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,  103}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,  103}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,  103}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,  103}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,  103}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,  103}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,  103}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,  103}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,  103}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,  103}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,  103}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,  103}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,  103}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,  103}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,  103}},
    {        Token::FUNC_IDENTIFIER, {           TA_REDUCE_USING_RULE,  103}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,  103}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,  103}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,  103}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,  103}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,  103}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,  103}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,  103}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,  103}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,  103}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,  103}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,  103}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state   36
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   51}},

// ///////////////////////////////////////////////////////////////////////////
// state   37
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   46}},

// ///////////////////////////////////////////////////////////////////////////
// state   38
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   99}},

// ///////////////////////////////////////////////////////////////////////////
// state   39
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   83}},

// ///////////////////////////////////////////////////////////////////////////
// state   40
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   88}},

// ///////////////////////////////////////////////////////////////////////////
// state   41
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   42
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state   43
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   19}},

// ///////////////////////////////////////////////////////////////////////////
// state   44
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,   31}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   45
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   72}},

// ///////////////////////////////////////////////////////////////////////////
// state   46
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   71}},

// ///////////////////////////////////////////////////////////////////////////
// state   47
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   74}},

// ///////////////////////////////////////////////////////////////////////////
// state   48
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   73}},

// ///////////////////////////////////////////////////////////////////////////
// state   49
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   75}},

// ///////////////////////////////////////////////////////////////////////////
// state   50
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   77}},

// ///////////////////////////////////////////////////////////////////////////
// state   51
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   23}},

// ///////////////////////////////////////////////////////////////////////////
// state   52
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   22}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   22}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   22}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   22}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   22}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   22}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   22}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   22}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   22}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   22}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   22}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   22}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   22}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   22}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  103}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   53
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   54
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   55
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state   56
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state   57
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   31}},

// ///////////////////////////////////////////////////////////////////////////
// state   58
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   30}},

// ///////////////////////////////////////////////////////////////////////////
// state   59
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state   60
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  106}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   61
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  107}},

// ///////////////////////////////////////////////////////////////////////////
// state   62
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  108}},

// ///////////////////////////////////////////////////////////////////////////
// state   63
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state   64
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  109}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  110}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   65
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  111}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  104}},

// ///////////////////////////////////////////////////////////////////////////
// state   66
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  112}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,  113}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  107}},

// ///////////////////////////////////////////////////////////////////////////
// state   67
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   82}},

// ///////////////////////////////////////////////////////////////////////////
// state   68
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   81}},

// ///////////////////////////////////////////////////////////////////////////
// state   69
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   86}},

// ///////////////////////////////////////////////////////////////////////////
// state   70
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   85}},

// ///////////////////////////////////////////////////////////////////////////
// state   71
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   76}},

// ///////////////////////////////////////////////////////////////////////////
// state   72
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   78}},

// ///////////////////////////////////////////////////////////////////////////
// state   73
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   90}},

// ///////////////////////////////////////////////////////////////////////////
// state   74
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   89}},

// ///////////////////////////////////////////////////////////////////////////
// state   75
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  114}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,  115}},

// ///////////////////////////////////////////////////////////////////////////
// state   76
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   92}},

// ///////////////////////////////////////////////////////////////////////////
// state   77
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  116}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   78
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  117}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  118}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   79
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  119}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   80
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   53}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,   53}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,   53}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,   53}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,   53}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,   53}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,   53}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,   53}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,   53}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,   53}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,   53}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,   53}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   53}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   53}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   53}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   53}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   53}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   53}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   53}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   53}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   53}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   53}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  120}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   81
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  121}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   82
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  122}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   83
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  123}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   84
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  124}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   85
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  125}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   86
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  126}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   87
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  127}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   88
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  128}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   89
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  129}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   90
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  130}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   91
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  131}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   92
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  132}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   93
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  133}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   94
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   84}},

// ///////////////////////////////////////////////////////////////////////////
// state   95
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   87}},

// ///////////////////////////////////////////////////////////////////////////
// state   96
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  134}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state   97
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  135}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state   98
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,  100}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,  100}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  136}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,  100}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,  100}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,  100}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,  100}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,  100}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,  100}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,  100}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,  100}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,  100}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,  100}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,  100}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,  100}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,  100}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,  100}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,  100}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,  100}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,  100}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,  100}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,  100}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,  100}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,  100}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,  100}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,  100}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,  100}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,  100}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,  100}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,  100}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,  100}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,  100}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  137}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {           Token::param_list__, {                  TA_PUSH_STATE,  138}},

// ///////////////////////////////////////////////////////////////////////////
// state   99
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   20}},

// ///////////////////////////////////////////////////////////////////////////
// state  100
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   70}},

// ///////////////////////////////////////////////////////////////////////////
// state  101
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   18}},

// ///////////////////////////////////////////////////////////////////////////
// state  102
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  139}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  103
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  140}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state  104
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   29}},

// ///////////////////////////////////////////////////////////////////////////
// state  105
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  141}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state  106
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  142}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state  107
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  143}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  144}},

// ///////////////////////////////////////////////////////////////////////////
// state  108
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  145}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   10}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   10}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  146}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  144}},

// ///////////////////////////////////////////////////////////////////////////
// state  109
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   35}},

// ///////////////////////////////////////////////////////////////////////////
// state  110
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  147}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  148}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  111
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  149}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  112
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  150}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  113
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  151}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  114
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  152}},

// ///////////////////////////////////////////////////////////////////////////
// state  115
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  153}},

// ///////////////////////////////////////////////////////////////////////////
// state  116
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   59}},

// ///////////////////////////////////////////////////////////////////////////
// state  117
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state  118
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  154}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state  119
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   54}},

// ///////////////////////////////////////////////////////////////////////////
// state  120
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   52}},

// ///////////////////////////////////////////////////////////////////////////
// state  121
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   55}},

// ///////////////////////////////////////////////////////////////////////////
// state  122
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   56}},

// ///////////////////////////////////////////////////////////////////////////
// state  123
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   60}},

// ///////////////////////////////////////////////////////////////////////////
// state  124
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   57}},

// ///////////////////////////////////////////////////////////////////////////
// state  125
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   58}},

// ///////////////////////////////////////////////////////////////////////////
// state  126
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   66}},

// ///////////////////////////////////////////////////////////////////////////
// state  127
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   65}},

// ///////////////////////////////////////////////////////////////////////////
// state  128
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   63}},

// ///////////////////////////////////////////////////////////////////////////
// state  129
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   64}},

// ///////////////////////////////////////////////////////////////////////////
// state  130
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   68}},

// ///////////////////////////////////////////////////////////////////////////
// state  131
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   67}},

// ///////////////////////////////////////////////////////////////////////////
// state  132
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   62}},

// ///////////////////////////////////////////////////////////////////////////
// state  133
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   61}},

// ///////////////////////////////////////////////////////////////////////////
// state  134
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   69}},

// ///////////////////////////////////////////////////////////////////////////
// state  135
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  101}},

// ///////////////////////////////////////////////////////////////////////////
// state  136
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   97}},

// ///////////////////////////////////////////////////////////////////////////
// state  137
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  109}},

// ///////////////////////////////////////////////////////////////////////////
// state  138
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  155}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  156}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   99}},

// ///////////////////////////////////////////////////////////////////////////
// state  139
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,  157}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  140
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,  158}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  141
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,  159}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

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
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,  160}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  143
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  161}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  162}},

// ///////////////////////////////////////////////////////////////////////////
// state  144
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},

// ///////////////////////////////////////////////////////////////////////////
// state  145
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  163}},

// ///////////////////////////////////////////////////////////////////////////
// state  146
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  164}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  162}},

// ///////////////////////////////////////////////////////////////////////////
// state  147
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state  148
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  165}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  166}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  167}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  149
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  105}},

// ///////////////////////////////////////////////////////////////////////////
// state  150
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  108}},

// ///////////////////////////////////////////////////////////////////////////
// state  151
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  168}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state  152
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  169}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  144}},

// ///////////////////////////////////////////////////////////////////////////
// state  153
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  170}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   10}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   10}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  171}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  144}},

// ///////////////////////////////////////////////////////////////////////////
// state  154
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state  155
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state  156
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   41}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  172}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  157
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   24}},

// ///////////////////////////////////////////////////////////////////////////
// state  158
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   21}},

// ///////////////////////////////////////////////////////////////////////////
// state  159
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  173}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   28}},

// ///////////////////////////////////////////////////////////////////////////
// state  160
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  174}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   26}},

// ///////////////////////////////////////////////////////////////////////////
// state  161
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  175}},

// ///////////////////////////////////////////////////////////////////////////
// state  162
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,  176}},

// ///////////////////////////////////////////////////////////////////////////
// state  163
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  177}},

// ///////////////////////////////////////////////////////////////////////////
// state  164
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  178}},

// ///////////////////////////////////////////////////////////////////////////
// state  165
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   39}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   40}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  166
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,  179}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  167
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  180}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  168
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state  169
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  181}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  162}},

// ///////////////////////////////////////////////////////////////////////////
// state  170
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  182}},

// ///////////////////////////////////////////////////////////////////////////
// state  171
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  183}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  162}},

// ///////////////////////////////////////////////////////////////////////////
// state  172
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   77}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   79}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   81}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   82}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   83}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   84}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   85}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  110}},

// ///////////////////////////////////////////////////////////////////////////
// state  173
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,  184}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  174
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,  185}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  175
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  186}},

// ///////////////////////////////////////////////////////////////////////////
// state  176
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   12}},

// ///////////////////////////////////////////////////////////////////////////
// state  177
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  187}},

// ///////////////////////////////////////////////////////////////////////////
// state  178
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  188}},

// ///////////////////////////////////////////////////////////////////////////
// state  179
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   33}},

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
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,  189}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  181
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  190}},

// ///////////////////////////////////////////////////////////////////////////
// state  182
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  191}},

// ///////////////////////////////////////////////////////////////////////////
// state  183
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  192}},

// ///////////////////////////////////////////////////////////////////////////
// state  184
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   27}},

// ///////////////////////////////////////////////////////////////////////////
// state  185
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   25}},

// ///////////////////////////////////////////////////////////////////////////
// state  186
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  193}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,   31}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  187
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  194}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,   31}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  188
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  195}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,   31}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  189
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   34}},

// ///////////////////////////////////////////////////////////////////////////
// state  190
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  196}},

// ///////////////////////////////////////////////////////////////////////////
// state  191
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  197}},

// ///////////////////////////////////////////////////////////////////////////
// state  192
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  198}},

// ///////////////////////////////////////////////////////////////////////////
// state  193
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    5}},

// ///////////////////////////////////////////////////////////////////////////
// state  194
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    3}},

// ///////////////////////////////////////////////////////////////////////////
// state  195
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    2}},

// ///////////////////////////////////////////////////////////////////////////
// state  196
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  199}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,   31}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  197
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  200}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,   31}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  198
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  201}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::BREAK, {        TA_SHIFT_AND_PUSH_STATE,   12}},
    {               Token::CONTINUE, {        TA_SHIFT_AND_PUSH_STATE,   13}},
    {                 Token::RETURN, {        TA_SHIFT_AND_PUSH_STATE,   14}},
    {                     Token::IF, {        TA_SHIFT_AND_PUSH_STATE,   15}},
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   16}},
    {        Token::FUNC_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   17}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {                    Token::FOR, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   21}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                  Token::FINAL, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   30}},
    {            Token::statement__, {                  TA_PUSH_STATE,   31}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   32}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   33}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   34}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   35}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   36}},
    {                 Token::call__, {                  TA_PUSH_STATE,   37}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state  199
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    6}},

// ///////////////////////////////////////////////////////////////////////////
// state  200
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    4}},

// ///////////////////////////////////////////////////////////////////////////
// state  201
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
	mErrors =  error_text + mErrors;
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

#line 5627 "SteelParser.cpp"

