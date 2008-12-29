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
        "BAREWORD",
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
        "GT",
        "GTE",
        "IF",
        "IMPORT",
        "INCREMENT",
        "INT",
        "LT",
        "LTE",
        "NE",
        "NOT",
        "OR",
        "POP",
        "RETURN",
        "SCOPE",
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

#line 139 "steel.trison"

				AstScript * pScript =   new AstScript(
							list->GetLine(),
							list->GetScript());
		        pScript->SetList(list);
				return pScript;
			
#line 513 "SteelParser.cpp"
}

// rule 2: func_definition <- FUNCTION func_identifier:id '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0002 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 152 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									false);
				
#line 535 "SteelParser.cpp"
}

// rule 3: func_definition <- FUNCTION func_identifier:id '(' %error ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0003 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 162 "steel.trison"

					AstFuncIdentifier *pId = static_cast<AstFuncIdentifier*>(id);
					addError(id->GetLine(),"parser error in parameter list for function '" + pId->getValue() + '\'');
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									pId,
									NULL,
									stmts,
									false);
				
#line 557 "SteelParser.cpp"
}

// rule 4: func_definition <- FINAL FUNCTION func_identifier:id '(' %error ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0004 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 174 "steel.trison"

					AstFuncIdentifier *pId = static_cast<AstFuncIdentifier*>(id);
					addError(id->GetLine(),"parser error in parameter list for function '" + pId->getValue() + '\'');
					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									pId,
									NULL,
									stmts,
									true);
				
#line 579 "SteelParser.cpp"
}

// rule 5: func_definition <- FUNCTION %error '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0005 ()
{
    assert(3 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(6 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 186 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									params,
									stmts,
									false);
				
#line 602 "SteelParser.cpp"
}

// rule 6: func_definition <- FINAL FUNCTION %error '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0006 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 199 "steel.trison"

					addError(GET_LINE(),"parser error after 'function' keyword.");
					return new AstFunctionDefinition(GET_LINE(),
									GET_SCRIPT(),
									new AstFuncIdentifier(GET_LINE(),
										GET_SCRIPT(),
										"__%%error%%__"),
									params,
									stmts,
									true);
				
#line 625 "SteelParser.cpp"
}

// rule 7: func_definition <- FINAL FUNCTION func_identifier:id '(' param_definition:params ')' '{' statement_list:stmts '}'    
AstBase* SteelParser::ReductionRuleHandler0007 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstParamDefinitionList* params = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(7 < m_reduction_rule_token_count);
    AstStatementList* stmts = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 7]);

#line 213 "steel.trison"

					return new AstFunctionDefinition(id->GetLine(),
									id->GetScript(),
									static_cast<AstFuncIdentifier*>(id),
									params,
									stmts,
									true);
				
#line 647 "SteelParser.cpp"
}

// rule 8: param_id <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0008 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 227 "steel.trison"
 return id; 
#line 658 "SteelParser.cpp"
}

// rule 9: param_id <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0009 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 229 "steel.trison"
 return id; 
#line 669 "SteelParser.cpp"
}

// rule 10: param_definition <-     
AstBase* SteelParser::ReductionRuleHandler0010 ()
{

#line 234 "steel.trison"

		 return new AstParamDefinitionList(GET_LINE(), GET_SCRIPT());
	
#line 680 "SteelParser.cpp"
}

// rule 11: param_definition <- vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0011 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 239 "steel.trison"

				AstParamDefinitionList * pList = new AstParamDefinitionList(decl->GetLine(),decl->GetScript());
				pList->add(static_cast<AstDeclaration*>(decl));
				return pList;		
			
#line 695 "SteelParser.cpp"
}

// rule 12: param_definition <- param_definition:list ',' vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0012 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamDefinitionList* list = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 246 "steel.trison"

				list->add(static_cast<AstDeclaration*>(decl));
				return list;
			
#line 711 "SteelParser.cpp"
}

// rule 13: param_definition <- param_definition:list %error    
AstBase* SteelParser::ReductionRuleHandler0013 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamDefinitionList* list = static_cast< AstParamDefinitionList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 252 "steel.trison"

				addError(list->GetLine(),"expected parameter definition");
				return list;
			
#line 725 "SteelParser.cpp"
}

// rule 14: statement_list <-     
AstBase* SteelParser::ReductionRuleHandler0014 ()
{

#line 261 "steel.trison"

				AstStatementList *pList = 
					new AstStatementList(m_scanner->getCurrentLine(),
										m_scanner->getScriptName());
				return pList;
			
#line 739 "SteelParser.cpp"
}

// rule 15: statement_list <- statement_list:list statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0015 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 269 "steel.trison"

					list->add( stmt ); 
					return list;
				
#line 755 "SteelParser.cpp"
}

// rule 16: statement <- %error    
AstBase* SteelParser::ReductionRuleHandler0016 ()
{

#line 277 "steel.trison"
 
			addError(GET_LINE(),"parse error");
			return new AstStatement(GET_LINE(),GET_SCRIPT());
		
#line 767 "SteelParser.cpp"
}

// rule 17: statement <- IMPORT:imp STRING:str ';'    
AstBase* SteelParser::ReductionRuleHandler0017 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* imp = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(1 < m_reduction_rule_token_count);
    AstString* str = static_cast< AstString* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 282 "steel.trison"
 return new AstImport(GET_LINE(),GET_SCRIPT(),str); 
#line 780 "SteelParser.cpp"
}

// rule 18: statement <- exp_statement:exp    
AstBase* SteelParser::ReductionRuleHandler0018 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 284 "steel.trison"
 return new AstExpressionStatement(exp->GetLine(),exp->GetScript(), exp); 
#line 791 "SteelParser.cpp"
}

// rule 19: statement <- func_definition:func    
AstBase* SteelParser::ReductionRuleHandler0019 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFunctionDefinition* func = static_cast< AstFunctionDefinition* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 286 "steel.trison"
 return func; 
#line 802 "SteelParser.cpp"
}

// rule 20: statement <- '{' statement_list:list '}'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0020 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatementList* list = static_cast< AstStatementList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 288 "steel.trison"
 return list; 
#line 813 "SteelParser.cpp"
}

// rule 21: statement <- '{' '}'    
AstBase* SteelParser::ReductionRuleHandler0021 ()
{

#line 290 "steel.trison"

			 return new AstStatement(GET_LINE(),GET_SCRIPT());
			
#line 824 "SteelParser.cpp"
}

// rule 22: statement <- vardecl:vardecl ';'    
AstBase* SteelParser::ReductionRuleHandler0022 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* vardecl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 294 "steel.trison"
 return vardecl; 
#line 835 "SteelParser.cpp"
}

// rule 23: statement <- %error vardecl:decl    
AstBase* SteelParser::ReductionRuleHandler0023 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 297 "steel.trison"

				addError(decl->GetLine(),"unexpected tokens found before variable declaration.");
				return decl;
			
#line 849 "SteelParser.cpp"
}

// rule 24: statement <- vardecl:decl %error ';'    
AstBase* SteelParser::ReductionRuleHandler0024 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstDeclaration* decl = static_cast< AstDeclaration* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 303 "steel.trison"

			addError(decl->GetLine(),"expected ';' after variable declaration.");
			return decl;
		
#line 863 "SteelParser.cpp"
}

// rule 25: statement <- WHILE '(' exp:exp ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0025 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 308 "steel.trison"
 return new AstWhileStatement(exp->GetLine(), exp->GetScript(),exp,stmt); 
#line 876 "SteelParser.cpp"
}

// rule 26: statement <- WHILE '('    
AstBase* SteelParser::ReductionRuleHandler0026 ()
{

#line 311 "steel.trison"
 
				addError(GET_LINE(),"expected ')'");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT())); 
			
#line 889 "SteelParser.cpp"
}

// rule 27: statement <- WHILE %error    
AstBase* SteelParser::ReductionRuleHandler0027 ()
{

#line 318 "steel.trison"
 
				addError(GET_LINE(),"missing loop condition.");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT())); 
			
#line 902 "SteelParser.cpp"
}

// rule 28: statement <- WHILE '(' %error ')' statement:stmt    
AstBase* SteelParser::ReductionRuleHandler0028 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 326 "steel.trison"
 
				addError(GET_LINE(),"error in loop expression.");
				return new AstWhileStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt);    
			
#line 916 "SteelParser.cpp"
}

// rule 29: statement <- DO statement:stmt WHILE '(' exp:condition ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0029 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 332 "steel.trison"

				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), condition, stmt);
	   
#line 931 "SteelParser.cpp"
}

// rule 30: statement <- DO statement:stmt WHILE '(' %error ')'    
AstBase* SteelParser::ReductionRuleHandler0030 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 337 "steel.trison"

				addError(GET_LINE(),"error in while condition.");
				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), NULL, stmt);
	   
#line 945 "SteelParser.cpp"
}

// rule 31: statement <- DO statement:stmt %error    
AstBase* SteelParser::ReductionRuleHandler0031 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 343 "steel.trison"

				addError(GET_LINE(),"error. do loop missing proper while condition.");
				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), NULL, stmt);
	   
#line 959 "SteelParser.cpp"
}

// rule 32: statement <- DO statement:stmt WHILE '(' %error    
AstBase* SteelParser::ReductionRuleHandler0032 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 350 "steel.trison"

				addError(GET_LINE(),"error, missing condition or no closing ')' found after while.");
				return new AstDoStatement(GET_LINE(), GET_SCRIPT(), NULL, NULL);
	   
#line 973 "SteelParser.cpp"
}

// rule 33: statement <- IF '(' exp:exp ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0033 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 357 "steel.trison"
 return new AstIfStatement(exp->GetLine(), exp->GetScript(),exp,stmt,elses);
#line 988 "SteelParser.cpp"
}

// rule 34: statement <- IF '(' exp:exp ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0034 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 359 "steel.trison"
 return new AstIfStatement(exp->GetLine(),exp->GetScript(),exp,stmt); 
#line 1001 "SteelParser.cpp"
}

// rule 35: statement <- IF '(' %error ')' statement:stmt ELSE statement:elses     %prec ELSE
AstBase* SteelParser::ReductionRuleHandler0035 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* elses = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 362 "steel.trison"

			addError(GET_LINE(),"parse error in if condition."); 
			return new AstIfStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt,elses);
		
#line 1017 "SteelParser.cpp"
}

// rule 36: statement <- IF '(' %error ')' statement:stmt     %prec NON_ELSE
AstBase* SteelParser::ReductionRuleHandler0036 ()
{
    assert(4 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 368 "steel.trison"

				addError(GET_LINE(),"parse error in if condition."); 
				return new AstIfStatement(GET_LINE(), GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()),stmt);
			
#line 1031 "SteelParser.cpp"
}

// rule 37: statement <- IF '(' %error    
AstBase* SteelParser::ReductionRuleHandler0037 ()
{

#line 374 "steel.trison"

			addError(GET_LINE(),"expected ')' after if condition.");
			return new AstIfStatement(GET_LINE(),GET_SCRIPT(),
				new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT()));
		
#line 1044 "SteelParser.cpp"
}

// rule 38: statement <- IF %error    
AstBase* SteelParser::ReductionRuleHandler0038 ()
{

#line 381 "steel.trison"

			addError(GET_LINE(),"expected opening '(' after 'if'");
			return new AstIfStatement(GET_LINE(),GET_SCRIPT(),
				new AstExpression(GET_LINE(),GET_SCRIPT()), new AstStatement(GET_LINE(),GET_SCRIPT()));

		
#line 1058 "SteelParser.cpp"
}

// rule 39: statement <- RETURN exp:exp ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0039 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 389 "steel.trison"
 return new AstReturnStatement(exp->GetLine(),exp->GetScript(),exp);
#line 1069 "SteelParser.cpp"
}

// rule 40: statement <- RETURN ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0040 ()
{

#line 392 "steel.trison"

				return new AstReturnStatement(GET_LINE(),GET_SCRIPT());
			
#line 1080 "SteelParser.cpp"
}

// rule 41: statement <- RETURN %error    
AstBase* SteelParser::ReductionRuleHandler0041 ()
{

#line 397 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'return'");
				return new AstReturnStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1092 "SteelParser.cpp"
}

// rule 42: statement <- RETURN    
AstBase* SteelParser::ReductionRuleHandler0042 ()
{

#line 403 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'return'");
				return new AstReturnStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1104 "SteelParser.cpp"
}

// rule 43: statement <- FOR '(' exp_statement:start exp_statement:condition ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0043 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(5 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 5]);

#line 410 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition, 
						new AstExpression (start->GetLine(),start->GetScript()), stmt); 
			
#line 1122 "SteelParser.cpp"
}

// rule 44: statement <- FOR '(' exp_statement:start exp_statement:condition exp:iteration ')' statement:stmt     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0044 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* iteration = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);
    assert(6 < m_reduction_rule_token_count);
    AstStatement* stmt = static_cast< AstStatement* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 6]);

#line 416 "steel.trison"

				return new AstLoopStatement(start->GetLine(),start->GetScript(),start,condition,iteration,stmt);
			
#line 1141 "SteelParser.cpp"
}

// rule 45: statement <- FOR '(' %error    
AstBase* SteelParser::ReductionRuleHandler0045 ()
{

#line 421 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1155 "SteelParser.cpp"
}

// rule 46: statement <- FOR '(' exp_statement:start %error    
AstBase* SteelParser::ReductionRuleHandler0046 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 429 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1171 "SteelParser.cpp"
}

// rule 47: statement <- FOR '(' exp_statement:start exp_statement:condition %error    
AstBase* SteelParser::ReductionRuleHandler0047 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 437 "steel.trison"

				addError(GET_LINE(),"malformed for loop.");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1189 "SteelParser.cpp"
}

// rule 48: statement <- FOR '(' exp_statement:start exp_statement:condition exp:iteration    
AstBase* SteelParser::ReductionRuleHandler0048 ()
{
    assert(2 < m_reduction_rule_token_count);
    AstExpression* start = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* condition = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);
    assert(4 < m_reduction_rule_token_count);
    AstExpression* iteration = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 4]);

#line 445 "steel.trison"

				addError(GET_LINE(),"malformed for loop. Expected ')'");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1209 "SteelParser.cpp"
}

// rule 49: statement <- FOR %error    
AstBase* SteelParser::ReductionRuleHandler0049 ()
{

#line 453 "steel.trison"

				addError(GET_LINE(),"malformed for loop. Expected opening '('");
				return new AstLoopStatement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstExpression(GET_LINE(),GET_SCRIPT()), new AstExpression(GET_LINE(),GET_SCRIPT()),
						new AstStatement(GET_LINE(),GET_SCRIPT()));
			
#line 1223 "SteelParser.cpp"
}

// rule 50: statement <- BREAK ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0050 ()
{

#line 462 "steel.trison"

				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1234 "SteelParser.cpp"
}

// rule 51: statement <- BREAK %error    
AstBase* SteelParser::ReductionRuleHandler0051 ()
{

#line 467 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'break'");
				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1246 "SteelParser.cpp"
}

// rule 52: statement <- BREAK    
AstBase* SteelParser::ReductionRuleHandler0052 ()
{

#line 473 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'break'");
				return new AstBreakStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1258 "SteelParser.cpp"
}

// rule 53: statement <- CONTINUE ';'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0053 ()
{

#line 481 "steel.trison"

				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1269 "SteelParser.cpp"
}

// rule 54: statement <- CONTINUE %error    
AstBase* SteelParser::ReductionRuleHandler0054 ()
{

#line 486 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'continue'");
				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1281 "SteelParser.cpp"
}

// rule 55: statement <- CONTINUE    
AstBase* SteelParser::ReductionRuleHandler0055 ()
{

#line 492 "steel.trison"

				addError(GET_LINE(),"expected ';' after 'continue'");
				return new AstContinueStatement(GET_LINE(),GET_SCRIPT()); 
			
#line 1293 "SteelParser.cpp"
}

// rule 56: exp <- call:call    
AstBase* SteelParser::ReductionRuleHandler0056 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstCallExpression* call = static_cast< AstCallExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 503 "steel.trison"
 return call; 
#line 1304 "SteelParser.cpp"
}

// rule 57: exp <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0057 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstInteger* i = static_cast< AstInteger* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 505 "steel.trison"
 return i;
#line 1315 "SteelParser.cpp"
}

// rule 58: exp <- FLOAT:f    
AstBase* SteelParser::ReductionRuleHandler0058 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFloat* f = static_cast< AstFloat* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 507 "steel.trison"
 return f; 
#line 1326 "SteelParser.cpp"
}

// rule 59: exp <- STRING:s    
AstBase* SteelParser::ReductionRuleHandler0059 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstString* s = static_cast< AstString* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 509 "steel.trison"
 return s; 
#line 1337 "SteelParser.cpp"
}

// rule 60: exp <- BOOLEAN:b    
AstBase* SteelParser::ReductionRuleHandler0060 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* b = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 511 "steel.trison"
 return b; 
#line 1348 "SteelParser.cpp"
}

// rule 61: exp <- var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0061 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 513 "steel.trison"
 return id; 
#line 1359 "SteelParser.cpp"
}

// rule 62: exp <- array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0062 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 515 "steel.trison"
 return id; 
#line 1370 "SteelParser.cpp"
}

// rule 63: exp <- exp:a '+' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0063 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 517 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,b); 
#line 1383 "SteelParser.cpp"
}

// rule 64: exp <- exp:a '+'     %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0064 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 519 "steel.trison"
 
				addError(a->GetLine(),"expected expression before '+'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::ADD,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1397 "SteelParser.cpp"
}

// rule 65: exp <- exp:a '-'     %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0065 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 524 "steel.trison"
 
				addError(a->GetLine(),"expected expression before '-'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1411 "SteelParser.cpp"
}

// rule 66: exp <- exp:a '*'     %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0066 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 529 "steel.trison"
 
				addError(a->GetLine(),"expected expression after '*'.");	
				return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,new AstExpression(GET_LINE(),GET_SCRIPT()));
			  
#line 1425 "SteelParser.cpp"
}

// rule 67: exp <- '*' exp:b     %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0067 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 534 "steel.trison"
 
				addError(b->GetLine(),"expected expression before '*'.");	
				return new AstBinOp(b->GetLine(),b->GetScript(),AstBinOp::MULT,new AstExpression(GET_LINE(),GET_SCRIPT()),b);
			  
#line 1439 "SteelParser.cpp"
}

// rule 68: exp <- exp:a '-' exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0068 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 540 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::SUB,a,b); 
#line 1452 "SteelParser.cpp"
}

// rule 69: exp <- exp:a '*' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0069 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 542 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MULT,a,b); 
#line 1465 "SteelParser.cpp"
}

// rule 70: exp <- exp:a '/' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0070 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 544 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::DIV,a,b); 
#line 1478 "SteelParser.cpp"
}

// rule 71: exp <- exp:a '%' exp:b    %left %prec MULT_DIV_MOD
AstBase* SteelParser::ReductionRuleHandler0071 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 546 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::MOD,a,b); 
#line 1491 "SteelParser.cpp"
}

// rule 72: exp <- exp:a D exp:b    %left %prec POW
AstBase* SteelParser::ReductionRuleHandler0072 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 548 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::D,a,b); 
#line 1504 "SteelParser.cpp"
}

// rule 73: exp <- exp:lvalue '=' exp:exp    %right %prec ASSIGNMENT
AstBase* SteelParser::ReductionRuleHandler0073 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 550 "steel.trison"
 return new AstVarAssignmentExpression(lvalue->GetLine(),lvalue->GetScript(),lvalue,exp); 
#line 1517 "SteelParser.cpp"
}

// rule 74: exp <- exp:a '^' exp:b    %right %prec POW
AstBase* SteelParser::ReductionRuleHandler0074 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 552 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::POW,a,b); 
#line 1530 "SteelParser.cpp"
}

// rule 75: exp <- exp:a OR exp:b    %left %prec OR
AstBase* SteelParser::ReductionRuleHandler0075 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 554 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::OR,a,b); 
#line 1543 "SteelParser.cpp"
}

// rule 76: exp <- exp:a AND exp:b    %left %prec AND
AstBase* SteelParser::ReductionRuleHandler0076 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 556 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::AND,a,b); 
#line 1556 "SteelParser.cpp"
}

// rule 77: exp <- exp:a EQ exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0077 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 558 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::EQ,a,b); 
#line 1569 "SteelParser.cpp"
}

// rule 78: exp <- exp:a NE exp:b    %left %prec EQ_NE
AstBase* SteelParser::ReductionRuleHandler0078 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 560 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::NE,a,b); 
#line 1582 "SteelParser.cpp"
}

// rule 79: exp <- exp:a LT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0079 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 562 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LT,a,b); 
#line 1595 "SteelParser.cpp"
}

// rule 80: exp <- exp:a GT exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0080 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 564 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GT,a,b); 
#line 1608 "SteelParser.cpp"
}

// rule 81: exp <- exp:a LTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0081 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 566 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::LTE,a,b); 
#line 1621 "SteelParser.cpp"
}

// rule 82: exp <- exp:a GTE exp:b    %left %prec COMPARATOR
AstBase* SteelParser::ReductionRuleHandler0082 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 568 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::GTE,a,b); 
#line 1634 "SteelParser.cpp"
}

// rule 83: exp <- exp:a CAT exp:b    %left %prec ADD_SUB
AstBase* SteelParser::ReductionRuleHandler0083 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* a = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* b = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 570 "steel.trison"
 return new AstBinOp(a->GetLine(),a->GetScript(),AstBinOp::CAT,a,b); 
#line 1647 "SteelParser.cpp"
}

// rule 84: exp <- '(' exp:exp ')'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0084 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 572 "steel.trison"
 return exp; 
#line 1658 "SteelParser.cpp"
}

// rule 85: exp <- '-' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0085 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 574 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::MINUS,exp); 
#line 1669 "SteelParser.cpp"
}

// rule 86: exp <- '+' exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0086 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 576 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::PLUS,exp); 
#line 1680 "SteelParser.cpp"
}

// rule 87: exp <- NOT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0087 ()
{

#line 578 "steel.trison"

						addError(GET_LINE(),"expected expression after unary minus.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::NOT,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1692 "SteelParser.cpp"
}

// rule 88: exp <- CAT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0088 ()
{

#line 586 "steel.trison"

						addError(GET_LINE(),"expected expression after ':'.");
						return new AstUnaryOp(GET_LINE(),GET_SCRIPT(),AstUnaryOp::CAT,new AstExpression(GET_LINE(),GET_SCRIPT()));
								  
#line 1704 "SteelParser.cpp"
}

// rule 89: exp <- NOT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0089 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 592 "steel.trison"
 return new AstUnaryOp(exp->GetLine(), exp->GetScript(), AstUnaryOp::NOT,exp); 
#line 1715 "SteelParser.cpp"
}

// rule 90: exp <- CAT exp:exp     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0090 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 594 "steel.trison"
 return new AstUnaryOp(exp->GetLine(),
exp->GetScript(), AstUnaryOp::CAT,exp); 
#line 1727 "SteelParser.cpp"
}

// rule 91: exp <- exp:lvalue '[' exp:index ']'     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0091 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* index = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 597 "steel.trison"
 return new AstArrayElement(lvalue->GetLine(),lvalue->GetScript(),lvalue,index); 
#line 1740 "SteelParser.cpp"
}

// rule 92: exp <- INCREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0092 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 599 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::PRE);
#line 1751 "SteelParser.cpp"
}

// rule 93: exp <- INCREMENT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0093 ()
{

#line 601 "steel.trison"

										addError(GET_LINE(),"expected lvalue after '++'");
										return new AstIncrement(GET_LINE(),GET_SCRIPT(),
												new AstExpression(GET_LINE(),GET_SCRIPT()),AstIncrement::PRE);
										
#line 1764 "SteelParser.cpp"
}

// rule 94: exp <- %error INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0094 ()
{

#line 607 "steel.trison"
 
									addError(GET_LINE(),"expected lvalue before '++'");
									return new AstIncrement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()), AstIncrement::POST);
	   							
#line 1776 "SteelParser.cpp"
}

// rule 95: exp <- exp:lvalue INCREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0095 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 613 "steel.trison"
 return new AstIncrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstIncrement::POST);
#line 1787 "SteelParser.cpp"
}

// rule 96: exp <- DECREMENT exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0096 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 615 "steel.trison"
 return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::PRE);
#line 1798 "SteelParser.cpp"
}

// rule 97: exp <- DECREMENT %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0097 ()
{

#line 617 "steel.trison"

										addError(GET_LINE(),"expected lvalue after '--'");
										return new AstDecrement(GET_LINE(),GET_SCRIPT(),
												new AstExpression(GET_LINE(),GET_SCRIPT()),AstDecrement::PRE);
										
#line 1811 "SteelParser.cpp"
}

// rule 98: exp <- exp:lvalue DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0098 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 624 "steel.trison"
 
									return new AstDecrement(lvalue->GetLine(),lvalue->GetScript(),lvalue, AstDecrement::POST);
									
#line 1824 "SteelParser.cpp"
}

// rule 99: exp <- %error DECREMENT     %prec PAREN
AstBase* SteelParser::ReductionRuleHandler0099 ()
{

#line 628 "steel.trison"
 
									addError(GET_LINE(),"expected lvalue before '--'");
									return new AstDecrement(GET_LINE(),GET_SCRIPT(), new AstExpression(GET_LINE(),GET_SCRIPT()), AstDecrement::POST);
									
#line 1836 "SteelParser.cpp"
}

// rule 100: exp <- POP exp:lvalue     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0100 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstExpression* lvalue = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 634 "steel.trison"
 return new AstPop(lvalue->GetLine(),lvalue->GetScript(),lvalue); 
#line 1847 "SteelParser.cpp"
}

// rule 101: exp <- POP %error     %prec UNARY
AstBase* SteelParser::ReductionRuleHandler0101 ()
{

#line 636 "steel.trison"

						addError(GET_LINE(),"expected expression after 'pop'.");
						return new AstPop(GET_LINE(),GET_SCRIPT(),new AstExpression(GET_LINE(),GET_SCRIPT()));
							  
#line 1859 "SteelParser.cpp"
}

// rule 102: exp_statement <- ';'    
AstBase* SteelParser::ReductionRuleHandler0102 ()
{

#line 645 "steel.trison"

			return new AstExpression(GET_LINE(),GET_SCRIPT()); 
		
#line 1870 "SteelParser.cpp"
}

// rule 103: exp_statement <- exp:exp ';'    
AstBase* SteelParser::ReductionRuleHandler0103 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 649 "steel.trison"
 return exp; 
#line 1881 "SteelParser.cpp"
}

// rule 104: int_literal <- INT:i    
AstBase* SteelParser::ReductionRuleHandler0104 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstInteger* i = static_cast< AstInteger* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 654 "steel.trison"
 return i; 
#line 1892 "SteelParser.cpp"
}

// rule 105: var_identifier <- VAR_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0105 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 659 "steel.trison"
 return id; 
#line 1903 "SteelParser.cpp"
}

// rule 106: func_identifier <- BAREWORD:id    
AstBase* SteelParser::ReductionRuleHandler0106 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBareword* id = static_cast< AstBareword* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 664 "steel.trison"

				AstFuncIdentifier * pId = new AstFuncIdentifier(GET_LINE(),GET_SCRIPT(),id->GetWord()); 
				return pId; 
				
#line 1917 "SteelParser.cpp"
}

// rule 107: func_identifier <- BAREWORD:ns SCOPE BAREWORD:func    
AstBase* SteelParser::ReductionRuleHandler0107 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBareword* ns = static_cast< AstBareword* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstBareword* func = static_cast< AstBareword* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 670 "steel.trison"

				AstFuncIdentifier * pId = new AstFuncIdentifier(GET_LINE(),GET_SCRIPT(),func->GetWord(),ns->GetWord());
				return pId;
				
#line 1933 "SteelParser.cpp"
}

// rule 108: array_identifier <- ARRAY_IDENTIFIER:id    
AstBase* SteelParser::ReductionRuleHandler0108 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstBase* id = static_cast< AstBase* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 678 "steel.trison"
 return id; 
#line 1944 "SteelParser.cpp"
}

// rule 109: call <- func_identifier:id '(' ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0109 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 684 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id);
#line 1955 "SteelParser.cpp"
}

// rule 110: call <- func_identifier:id '(' param_list:params ')'     %prec CORRECT
AstBase* SteelParser::ReductionRuleHandler0110 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 686 "steel.trison"
 return new AstCallExpression(id->GetLine(),id->GetScript(),id,params);
#line 1968 "SteelParser.cpp"
}

// rule 111: call <- func_identifier:id '(' param_list:params    
AstBase* SteelParser::ReductionRuleHandler0111 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstParamList* params = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 689 "steel.trison"

				addError(GET_LINE(),"expected ')' before ';'.");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id,params); 
			
#line 1984 "SteelParser.cpp"
}

// rule 112: call <- func_identifier:id '('    
AstBase* SteelParser::ReductionRuleHandler0112 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 695 "steel.trison"

				addError(GET_LINE(),"expected ')' before ';'");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 1998 "SteelParser.cpp"
}

// rule 113: call <- func_identifier:id %error ')'    
AstBase* SteelParser::ReductionRuleHandler0113 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 701 "steel.trison"

				addError(GET_LINE(),"missing '(' in function call");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 2012 "SteelParser.cpp"
}

// rule 114: call <- func_identifier:id %error    
AstBase* SteelParser::ReductionRuleHandler0114 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 707 "steel.trison"

				addError(GET_LINE(),"function call missing parentheses.");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 2026 "SteelParser.cpp"
}

// rule 115: call <- func_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0115 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstFuncIdentifier* id = static_cast< AstFuncIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 713 "steel.trison"

				addError(GET_LINE(),"invalid bareword. function call missing parentheses?");
				return new AstCallExpression(id->GetLine(),id->GetScript(),id); 
			
#line 2040 "SteelParser.cpp"
}

// rule 116: vardecl <- VAR var_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0116 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 722 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id);
#line 2051 "SteelParser.cpp"
}

// rule 117: vardecl <- VAR var_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0117 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 724 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id,exp); 
#line 2064 "SteelParser.cpp"
}

// rule 118: vardecl <- CONSTANT var_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0118 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstVarIdentifier * id = static_cast< AstVarIdentifier * >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 726 "steel.trison"
 return new AstVarDeclaration(id->GetLine(),id->GetScript(),id,true,exp); 
#line 2077 "SteelParser.cpp"
}

// rule 119: vardecl <- VAR array_identifier:id '[' exp:i ']'    
AstBase* SteelParser::ReductionRuleHandler0119 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* i = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 728 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id,i); 
#line 2090 "SteelParser.cpp"
}

// rule 120: vardecl <- VAR array_identifier:id    
AstBase* SteelParser::ReductionRuleHandler0120 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);

#line 730 "steel.trison"
 return new AstArrayDeclaration(id->GetLine(),id->GetScript(),id); 
#line 2101 "SteelParser.cpp"
}

// rule 121: vardecl <- VAR array_identifier:id '=' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0121 ()
{
    assert(1 < m_reduction_rule_token_count);
    AstArrayIdentifier* id = static_cast< AstArrayIdentifier* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 1]);
    assert(3 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 3]);

#line 732 "steel.trison"

							AstArrayDeclaration *pDecl =  new AstArrayDeclaration(id->GetLine(),id->GetScript(),id);
							pDecl->assign(exp);
							return pDecl;
						 
#line 2118 "SteelParser.cpp"
}

// rule 122: param_list <- exp:exp    
AstBase* SteelParser::ReductionRuleHandler0122 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);

#line 741 "steel.trison"
 AstParamList * pList = new AstParamList ( exp->GetLine(), exp->GetScript() );
		  pList->add(exp);
		  return pList;
		
#line 2132 "SteelParser.cpp"
}

// rule 123: param_list <- param_list:list ',' exp:exp    
AstBase* SteelParser::ReductionRuleHandler0123 ()
{
    assert(0 < m_reduction_rule_token_count);
    AstParamList* list = static_cast< AstParamList* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 0]);
    assert(2 < m_reduction_rule_token_count);
    AstExpression* exp = static_cast< AstExpression* >(m_token_stack[m_token_stack.size() - m_reduction_rule_token_count + 2]);

#line 746 "steel.trison"
 list->add(exp); return list;
#line 2145 "SteelParser.cpp"
}



// ///////////////////////////////////////////////////////////////////////////
// reduction rule lookup table
// ///////////////////////////////////////////////////////////////////////////

SteelParser::ReductionRule const SteelParser::ms_reduction_rule[] =
{
    {                 Token::START_,  2, &SteelParser::ReductionRuleHandler0000, "rule 0: %start <- root END_    "},
    {                 Token::root__,  1, &SteelParser::ReductionRuleHandler0001, "rule 1: root <- statement_list    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0002, "rule 2: func_definition <- FUNCTION func_identifier '(' param_definition ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0003, "rule 3: func_definition <- FUNCTION func_identifier '(' %error ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  9, &SteelParser::ReductionRuleHandler0004, "rule 4: func_definition <- FINAL FUNCTION func_identifier '(' %error ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  8, &SteelParser::ReductionRuleHandler0005, "rule 5: func_definition <- FUNCTION %error '(' param_definition ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  9, &SteelParser::ReductionRuleHandler0006, "rule 6: func_definition <- FINAL FUNCTION %error '(' param_definition ')' '{' statement_list '}'    "},
    {      Token::func_definition__,  9, &SteelParser::ReductionRuleHandler0007, "rule 7: func_definition <- FINAL FUNCTION func_identifier '(' param_definition ')' '{' statement_list '}'    "},
    {             Token::param_id__,  1, &SteelParser::ReductionRuleHandler0008, "rule 8: param_id <- VAR_IDENTIFIER    "},
    {             Token::param_id__,  1, &SteelParser::ReductionRuleHandler0009, "rule 9: param_id <- ARRAY_IDENTIFIER    "},
    {     Token::param_definition__,  0, &SteelParser::ReductionRuleHandler0010, "rule 10: param_definition <-     "},
    {     Token::param_definition__,  1, &SteelParser::ReductionRuleHandler0011, "rule 11: param_definition <- vardecl    "},
    {     Token::param_definition__,  3, &SteelParser::ReductionRuleHandler0012, "rule 12: param_definition <- param_definition ',' vardecl    "},
    {     Token::param_definition__,  2, &SteelParser::ReductionRuleHandler0013, "rule 13: param_definition <- param_definition %error    "},
    {       Token::statement_list__,  0, &SteelParser::ReductionRuleHandler0014, "rule 14: statement_list <-     "},
    {       Token::statement_list__,  2, &SteelParser::ReductionRuleHandler0015, "rule 15: statement_list <- statement_list statement    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0016, "rule 16: statement <- %error    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0017, "rule 17: statement <- IMPORT STRING ';'    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0018, "rule 18: statement <- exp_statement    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0019, "rule 19: statement <- func_definition    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0020, "rule 20: statement <- '{' statement_list '}'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0021, "rule 21: statement <- '{' '}'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0022, "rule 22: statement <- vardecl ';'    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0023, "rule 23: statement <- %error vardecl    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0024, "rule 24: statement <- vardecl %error ';'    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0025, "rule 25: statement <- WHILE '(' exp ')' statement     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0026, "rule 26: statement <- WHILE '('    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0027, "rule 27: statement <- WHILE %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0028, "rule 28: statement <- WHILE '(' %error ')' statement    "},
    {            Token::statement__,  6, &SteelParser::ReductionRuleHandler0029, "rule 29: statement <- DO statement WHILE '(' exp ')'     %prec CORRECT"},
    {            Token::statement__,  6, &SteelParser::ReductionRuleHandler0030, "rule 30: statement <- DO statement WHILE '(' %error ')'    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0031, "rule 31: statement <- DO statement %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0032, "rule 32: statement <- DO statement WHILE '(' %error    "},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0033, "rule 33: statement <- IF '(' exp ')' statement ELSE statement     %prec ELSE"},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0034, "rule 34: statement <- IF '(' exp ')' statement     %prec NON_ELSE"},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0035, "rule 35: statement <- IF '(' %error ')' statement ELSE statement     %prec ELSE"},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0036, "rule 36: statement <- IF '(' %error ')' statement     %prec NON_ELSE"},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0037, "rule 37: statement <- IF '(' %error    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0038, "rule 38: statement <- IF %error    "},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0039, "rule 39: statement <- RETURN exp ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0040, "rule 40: statement <- RETURN ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0041, "rule 41: statement <- RETURN %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0042, "rule 42: statement <- RETURN    "},
    {            Token::statement__,  6, &SteelParser::ReductionRuleHandler0043, "rule 43: statement <- FOR '(' exp_statement exp_statement ')' statement     %prec CORRECT"},
    {            Token::statement__,  7, &SteelParser::ReductionRuleHandler0044, "rule 44: statement <- FOR '(' exp_statement exp_statement exp ')' statement     %prec CORRECT"},
    {            Token::statement__,  3, &SteelParser::ReductionRuleHandler0045, "rule 45: statement <- FOR '(' %error    "},
    {            Token::statement__,  4, &SteelParser::ReductionRuleHandler0046, "rule 46: statement <- FOR '(' exp_statement %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0047, "rule 47: statement <- FOR '(' exp_statement exp_statement %error    "},
    {            Token::statement__,  5, &SteelParser::ReductionRuleHandler0048, "rule 48: statement <- FOR '(' exp_statement exp_statement exp    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0049, "rule 49: statement <- FOR %error    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0050, "rule 50: statement <- BREAK ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0051, "rule 51: statement <- BREAK %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0052, "rule 52: statement <- BREAK    "},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0053, "rule 53: statement <- CONTINUE ';'     %prec CORRECT"},
    {            Token::statement__,  2, &SteelParser::ReductionRuleHandler0054, "rule 54: statement <- CONTINUE %error    "},
    {            Token::statement__,  1, &SteelParser::ReductionRuleHandler0055, "rule 55: statement <- CONTINUE    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0056, "rule 56: exp <- call    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0057, "rule 57: exp <- INT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0058, "rule 58: exp <- FLOAT    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0059, "rule 59: exp <- STRING    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0060, "rule 60: exp <- BOOLEAN    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0061, "rule 61: exp <- var_identifier    "},
    {                  Token::exp__,  1, &SteelParser::ReductionRuleHandler0062, "rule 62: exp <- array_identifier    "},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0063, "rule 63: exp <- exp '+' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0064, "rule 64: exp <- exp '+'     %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0065, "rule 65: exp <- exp '-'     %prec ADD_SUB"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0066, "rule 66: exp <- exp '*'     %prec MULT_DIV_MOD"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0067, "rule 67: exp <- '*' exp     %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0068, "rule 68: exp <- exp '-' exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0069, "rule 69: exp <- exp '*' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0070, "rule 70: exp <- exp '/' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0071, "rule 71: exp <- exp '%' exp    %left %prec MULT_DIV_MOD"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0072, "rule 72: exp <- exp D exp    %left %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0073, "rule 73: exp <- exp '=' exp    %right %prec ASSIGNMENT"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0074, "rule 74: exp <- exp '^' exp    %right %prec POW"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0075, "rule 75: exp <- exp OR exp    %left %prec OR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0076, "rule 76: exp <- exp AND exp    %left %prec AND"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0077, "rule 77: exp <- exp EQ exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0078, "rule 78: exp <- exp NE exp    %left %prec EQ_NE"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0079, "rule 79: exp <- exp LT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0080, "rule 80: exp <- exp GT exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0081, "rule 81: exp <- exp LTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0082, "rule 82: exp <- exp GTE exp    %left %prec COMPARATOR"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0083, "rule 83: exp <- exp CAT exp    %left %prec ADD_SUB"},
    {                  Token::exp__,  3, &SteelParser::ReductionRuleHandler0084, "rule 84: exp <- '(' exp ')'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0085, "rule 85: exp <- '-' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0086, "rule 86: exp <- '+' exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0087, "rule 87: exp <- NOT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0088, "rule 88: exp <- CAT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0089, "rule 89: exp <- NOT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0090, "rule 90: exp <- CAT exp     %prec UNARY"},
    {                  Token::exp__,  4, &SteelParser::ReductionRuleHandler0091, "rule 91: exp <- exp '[' exp ']'     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0092, "rule 92: exp <- INCREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0093, "rule 93: exp <- INCREMENT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0094, "rule 94: exp <- %error INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0095, "rule 95: exp <- exp INCREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0096, "rule 96: exp <- DECREMENT exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0097, "rule 97: exp <- DECREMENT %error     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0098, "rule 98: exp <- exp DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0099, "rule 99: exp <- %error DECREMENT     %prec PAREN"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0100, "rule 100: exp <- POP exp     %prec UNARY"},
    {                  Token::exp__,  2, &SteelParser::ReductionRuleHandler0101, "rule 101: exp <- POP %error     %prec UNARY"},
    {        Token::exp_statement__,  1, &SteelParser::ReductionRuleHandler0102, "rule 102: exp_statement <- ';'    "},
    {        Token::exp_statement__,  2, &SteelParser::ReductionRuleHandler0103, "rule 103: exp_statement <- exp ';'    "},
    {          Token::int_literal__,  1, &SteelParser::ReductionRuleHandler0104, "rule 104: int_literal <- INT    "},
    {       Token::var_identifier__,  1, &SteelParser::ReductionRuleHandler0105, "rule 105: var_identifier <- VAR_IDENTIFIER    "},
    {      Token::func_identifier__,  1, &SteelParser::ReductionRuleHandler0106, "rule 106: func_identifier <- BAREWORD    "},
    {      Token::func_identifier__,  3, &SteelParser::ReductionRuleHandler0107, "rule 107: func_identifier <- BAREWORD SCOPE BAREWORD    "},
    {     Token::array_identifier__,  1, &SteelParser::ReductionRuleHandler0108, "rule 108: array_identifier <- ARRAY_IDENTIFIER    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0109, "rule 109: call <- func_identifier '(' ')'     %prec CORRECT"},
    {                 Token::call__,  4, &SteelParser::ReductionRuleHandler0110, "rule 110: call <- func_identifier '(' param_list ')'     %prec CORRECT"},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0111, "rule 111: call <- func_identifier '(' param_list    "},
    {                 Token::call__,  2, &SteelParser::ReductionRuleHandler0112, "rule 112: call <- func_identifier '('    "},
    {                 Token::call__,  3, &SteelParser::ReductionRuleHandler0113, "rule 113: call <- func_identifier %error ')'    "},
    {                 Token::call__,  2, &SteelParser::ReductionRuleHandler0114, "rule 114: call <- func_identifier %error    "},
    {                 Token::call__,  1, &SteelParser::ReductionRuleHandler0115, "rule 115: call <- func_identifier    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0116, "rule 116: vardecl <- VAR var_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0117, "rule 117: vardecl <- VAR var_identifier '=' exp    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0118, "rule 118: vardecl <- CONSTANT var_identifier '=' exp    "},
    {              Token::vardecl__,  5, &SteelParser::ReductionRuleHandler0119, "rule 119: vardecl <- VAR array_identifier '[' exp ']'    "},
    {              Token::vardecl__,  2, &SteelParser::ReductionRuleHandler0120, "rule 120: vardecl <- VAR array_identifier    "},
    {              Token::vardecl__,  4, &SteelParser::ReductionRuleHandler0121, "rule 121: vardecl <- VAR array_identifier '=' exp    "},
    {           Token::param_list__,  1, &SteelParser::ReductionRuleHandler0122, "rule 122: param_list <- exp    "},
    {           Token::param_list__,  3, &SteelParser::ReductionRuleHandler0123, "rule 123: param_list <- param_list ',' exp    "},

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
    {   5,   32,    0,   37,    9}, // state    2
    {   0,    0,   46,    0,    0}, // state    3
    {  47,    4,   51,   52,    1}, // state    4
    {   0,    0,   53,    0,    0}, // state    5
    {  54,   17,    0,   71,    5}, // state    6
    {  76,    1,   77,   78,    1}, // state    7
    {  79,   17,    0,   96,    5}, // state    8
    { 101,   17,    0,  118,    5}, // state    9
    { 123,   17,    0,  140,    5}, // state   10
    { 145,   17,    0,  162,    5}, // state   11
    { 167,    2,    0,    0,    0}, // state   12
    { 169,   34,    0,    0,    0}, // state   13
    { 203,   34,    0,    0,    0}, // state   14
    { 237,   34,    0,  271,    5}, // state   15
    { 276,    2,    0,    0,    0}, // state   16
    { 278,    2,    0,  280,    1}, // state   17
    { 281,    1,  282,    0,    0}, // state   18
    {   0,    0,  283,    0,    0}, // state   19
    {   0,    0,  284,    0,    0}, // state   20
    { 285,    2,    0,    0,    0}, // state   21
    { 287,    2,    0,  289,    2}, // state   22
    {   0,    0,  291,    0,    0}, // state   23
    {   0,    0,  292,    0,    0}, // state   24
    {   0,    0,  293,    0,    0}, // state   25
    {   0,    0,  294,    0,    0}, // state   26
    { 295,   17,    0,  312,    5}, // state   27
    { 317,   17,    0,  334,    5}, // state   28
    { 339,   17,    0,  356,    5}, // state   29
    { 361,   17,    0,  378,    5}, // state   30
    { 383,    1,    0,    0,    0}, // state   31
    { 384,    1,    0,  385,    1}, // state   32
    { 386,   31,    0,  417,    9}, // state   33
    { 426,    1,    0,    0,    0}, // state   34
    {   0,    0,  427,    0,    0}, // state   35
    {   0,    0,  428,    0,    0}, // state   36
    { 429,   21,    0,    0,    0}, // state   37
    {   0,    0,  450,    0,    0}, // state   38
    {   0,    0,  451,    0,    0}, // state   39
    { 452,   51,    0,    0,    0}, // state   40
    {   0,    0,  503,    0,    0}, // state   41
    {   0,    0,  504,    0,    0}, // state   42
    { 505,    2,    0,    0,    0}, // state   43
    {   0,    0,  507,    0,    0}, // state   44
    {   0,    0,  508,    0,    0}, // state   45
    {   0,    0,  509,    0,    0}, // state   46
    { 510,    2,    0,    0,    0}, // state   47
    { 512,   21,    0,    0,    0}, // state   48
    {   0,    0,  533,    0,    0}, // state   49
    { 534,   32,    0,  566,    9}, // state   50
    { 575,    5,  580,    0,    0}, // state   51
    { 581,    5,  586,    0,    0}, // state   52
    { 587,    8,  595,    0,    0}, // state   53
    { 596,    2,  598,    0,    0}, // state   54
    { 599,    5,  604,    0,    0}, // state   55
    {   0,    0,  605,    0,    0}, // state   56
    { 606,   34,    0,  640,    5}, // state   57
    {   0,    0,  645,    0,    0}, // state   58
    {   0,    0,  646,    0,    0}, // state   59
    {   0,    0,  647,    0,    0}, // state   60
    {   0,    0,  648,    0,    0}, // state   61
    { 649,    2,  651,    0,    0}, // state   62
    {   0,    0,  652,    0,    0}, // state   63
    { 653,   21,    0,    0,    0}, // state   64
    {   0,    0,  674,    0,    0}, // state   65
    { 675,   17,    0,  692,    5}, // state   66
    { 697,    1,    0,    0,    0}, // state   67
    { 698,    1,    0,    0,    0}, // state   68
    { 699,    1,    0,    0,    0}, // state   69
    {   0,    0,  700,    0,    0}, // state   70
    { 701,   18,    0,  719,    6}, // state   71
    { 725,    1,  726,    0,    0}, // state   72
    { 727,    2,  729,    0,    0}, // state   73
    { 730,    2,  732,    0,    0}, // state   74
    { 733,    5,  738,    0,    0}, // state   75
    { 739,    2,  741,    0,    0}, // state   76
    { 742,    5,  747,    0,    0}, // state   77
    { 748,    2,  750,    0,    0}, // state   78
    { 751,    5,  756,    0,    0}, // state   79
    { 757,    2,  759,    0,    0}, // state   80
    { 760,    5,  765,    0,    0}, // state   81
    { 766,    2,    0,  768,    1}, // state   82
    { 769,    1,    0,    0,    0}, // state   83
    { 770,    2,    0,    0,    0}, // state   84
    { 772,    1,    0,    0,    0}, // state   85
    {   0,    0,  773,    0,    0}, // state   86
    { 774,   17,    0,  791,    5}, // state   87
    { 796,   17,    0,  813,    5}, // state   88
    { 818,   51,    0,  869,    5}, // state   89
    { 874,   51,    0,  925,    5}, // state   90
    { 930,   51,    0,  981,    5}, // state   91
    { 986,   17,    0, 1003,    5}, // state   92
    {1008,   17,    0, 1025,    5}, // state   93
    {1030,   17,    0, 1047,    5}, // state   94
    {1052,   17,    0, 1069,    5}, // state   95
    {1074,   17,    0, 1091,    5}, // state   96
    {1096,   17,    0, 1113,    5}, // state   97
    {1118,   17,    0, 1135,    5}, // state   98
    {1140,   17,    0, 1157,    5}, // state   99
    {1162,   17,    0, 1179,    5}, // state  100
    {1184,   17,    0, 1201,    5}, // state  101
    {1206,   17,    0, 1223,    5}, // state  102
    {1228,   17,    0, 1245,    5}, // state  103
    {   0,    0, 1250,    0,    0}, // state  104
    {   0,    0, 1251,    0,    0}, // state  105
    {1252,   17,    0, 1269,    5}, // state  106
    {1274,    1, 1275,    0,    0}, // state  107
    {1276,   51,    0, 1327,    6}, // state  108
    {1333,    1,    0,    0,    0}, // state  109
    {   0,    0, 1334,    0,    0}, // state  110
    {   0,    0, 1335,    0,    0}, // state  111
    {   0,    0, 1336,    0,    0}, // state  112
    {1337,    3,    0,    0,    0}, // state  113
    {1340,   21,    0,    0,    0}, // state  114
    {   0,    0, 1361,    0,    0}, // state  115
    {1362,    3, 1365,    0,    0}, // state  116
    {1366,   21,    0,    0,    0}, // state  117
    {1387,    2, 1389, 1390,    2}, // state  118
    {1392,    5,    0, 1397,    2}, // state  119
    {   0,    0, 1399,    0,    0}, // state  120
    {1400,    2, 1402,    0,    0}, // state  121
    {1403,   18,    0, 1421,    6}, // state  122
    {1427,   17,    0, 1444,    5}, // state  123
    {1449,   17,    0, 1466,    5}, // state  124
    {1471,   17,    0, 1488,    5}, // state  125
    {1493,    1,    0,    0,    0}, // state  126
    {1494,    1,    0,    0,    0}, // state  127
    {1495,   17,    0, 1512,    5}, // state  128
    {   0,    0, 1517,    0,    0}, // state  129
    {1518,    1,    0,    0,    0}, // state  130
    {   0,    0, 1519,    0,    0}, // state  131
    {1520,   20, 1540,    0,    0}, // state  132
    {1541,   21,    0,    0,    0}, // state  133
    {1562,    9, 1571,    0,    0}, // state  134
    {1572,    9, 1581,    0,    0}, // state  135
    {1582,    6, 1588,    0,    0}, // state  136
    {1589,    6, 1595,    0,    0}, // state  137
    {1596,    4, 1600,    0,    0}, // state  138
    {1601,    6, 1607,    0,    0}, // state  139
    {1608,    4, 1612,    0,    0}, // state  140
    {1613,   11, 1624,    0,    0}, // state  141
    {1625,   11, 1636,    0,    0}, // state  142
    {1637,   15, 1652,    0,    0}, // state  143
    {1653,   15, 1668,    0,    0}, // state  144
    {1669,   11, 1680,    0,    0}, // state  145
    {1681,   11, 1692,    0,    0}, // state  146
    {1693,   17, 1710,    0,    0}, // state  147
    {1711,   18, 1729,    0,    0}, // state  148
    {1730,    9, 1739,    0,    0}, // state  149
    {   0,    0, 1740,    0,    0}, // state  150
    {   0,    0, 1741,    0,    0}, // state  151
    {1742,   20, 1762,    0,    0}, // state  152
    {1763,    2, 1765,    0,    0}, // state  153
    {   0,    0, 1766,    0,    0}, // state  154
    {1767,   31,    0, 1798,    9}, // state  155
    {1807,   31,    0, 1838,    9}, // state  156
    {1847,   31,    0, 1878,    9}, // state  157
    {1887,   31,    0, 1918,    9}, // state  158
    {1927,    3,    0,    0,    0}, // state  159
    {   0,    0, 1930,    0,    0}, // state  160
    {1931,    1,    0,    0,    0}, // state  161
    {1932,    3,    0,    0,    0}, // state  162
    {1935,    2, 1937,    0,    0}, // state  163
    {1938,   18,    0, 1956,    5}, // state  164
    {1961,   20, 1981,    0,    0}, // state  165
    {1982,   20, 2002,    0,    0}, // state  166
    {2003,   21,    0,    0,    0}, // state  167
    {2024,    2, 2026, 2027,    2}, // state  168
    {2029,    5,    0, 2034,    2}, // state  169
    {2036,   20, 2056,    0,    0}, // state  170
    {2057,   17,    0, 2074,    5}, // state  171
    {   0,    0, 2079,    0,    0}, // state  172
    {   0,    0, 2080,    0,    0}, // state  173
    {2081,   17,    0, 2098,    5}, // state  174
    {   0,    0, 2103,    0,    0}, // state  175
    {   0,    0, 2104,    0,    0}, // state  176
    {2105,    1, 2106,    0,    0}, // state  177
    {2107,    1, 2108,    0,    0}, // state  178
    {   0,    0, 2109,    0,    0}, // state  179
    {2110,    1,    0,    0,    0}, // state  180
    {2111,    2,    0, 2113,    1}, // state  181
    {2114,    1,    0,    0,    0}, // state  182
    {2115,    1,    0,    0,    0}, // state  183
    {2116,    2, 2118,    0,    0}, // state  184
    {2119,   31,    0, 2150,    9}, // state  185
    {2159,   21, 2180,    0,    0}, // state  186
    {   0,    0, 2181,    0,    0}, // state  187
    {2182,    3,    0,    0,    0}, // state  188
    {2185,    1,    0,    0,    0}, // state  189
    {2186,    3,    0,    0,    0}, // state  190
    {2189,    3, 2192,    0,    0}, // state  191
    {2193,   21,    0,    0,    0}, // state  192
    {2214,   20, 2234,    0,    0}, // state  193
    {2235,   31,    0, 2266,    9}, // state  194
    {2275,   31,    0, 2306,    9}, // state  195
    {   0,    0, 2315, 2316,    1}, // state  196
    {   0,    0, 2317,    0,    0}, // state  197
    {   0,    0, 2318, 2319,    1}, // state  198
    {   0,    0, 2320, 2321,    1}, // state  199
    {   0,    0, 2322,    0,    0}, // state  200
    {2323,   31,    0, 2354,    9}, // state  201
    {2363,    1,    0,    0,    0}, // state  202
    {2364,    1,    0,    0,    0}, // state  203
    {2365,    1,    0,    0,    0}, // state  204
    {   0,    0, 2366,    0,    0}, // state  205
    {   0,    0, 2367,    0,    0}, // state  206
    {   0,    0, 2368,    0,    0}, // state  207
    {   0,    0, 2369,    0,    0}, // state  208
    {2370,   32,    0, 2402,    9}, // state  209
    {2411,   32,    0, 2443,    9}, // state  210
    {2452,   32,    0, 2484,    9}, // state  211
    {   0,    0, 2493,    0,    0}, // state  212
    {   0,    0, 2494, 2495,    1}, // state  213
    {   0,    0, 2496, 2497,    1}, // state  214
    {   0,    0, 2498, 2499,    1}, // state  215
    {   0,    0, 2500,    0,    0}, // state  216
    {   0,    0, 2501,    0,    0}, // state  217
    {   0,    0, 2502,    0,    0}, // state  218
    {2503,   32,    0, 2535,    9}, // state  219
    {2544,   32,    0, 2576,    9}, // state  220
    {2585,   32,    0, 2617,    9}, // state  221
    {   0,    0, 2626,    0,    0}, // state  222
    {   0,    0, 2627,    0,    0}, // state  223
    {   0,    0, 2628,    0,    0}  // state  224

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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,   36}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

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
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   16}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,   46}},

// ///////////////////////////////////////////////////////////////////////////
// state    5
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  102}},

// ///////////////////////////////////////////////////////////////////////////
// state    6
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   48}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state    7
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,   49}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,   50}},

// ///////////////////////////////////////////////////////////////////////////
// state    8
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state    9
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   10
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   53}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   11
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   54}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   55}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   12
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   56}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   57}},

// ///////////////////////////////////////////////////////////////////////////
// state   13
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   52}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   58}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   59}},
    {              Token::Type('('), {           TA_REDUCE_USING_RULE,   52}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   52}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   52}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,   52}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,   52}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,   52}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,   52}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   52}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   52}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   52}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   52}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   52}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   52}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   52}},
    {               Token::BAREWORD, {           TA_REDUCE_USING_RULE,   52}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,   52}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,   52}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   52}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   52}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,   52}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,   52}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,   52}},
    {                Token::BOOLEAN, {           TA_REDUCE_USING_RULE,   52}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,   52}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,   52}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,   52}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,   52}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   52}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   52}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   52}},
    {                 Token::IMPORT, {           TA_REDUCE_USING_RULE,   52}},

// ///////////////////////////////////////////////////////////////////////////
// state   14
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   55}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   60}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   61}},
    {              Token::Type('('), {           TA_REDUCE_USING_RULE,   55}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   55}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   55}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,   55}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,   55}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,   55}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,   55}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   55}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   55}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   55}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   55}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   55}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   55}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   55}},
    {               Token::BAREWORD, {           TA_REDUCE_USING_RULE,   55}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,   55}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,   55}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   55}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   55}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,   55}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,   55}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,   55}},
    {                Token::BOOLEAN, {           TA_REDUCE_USING_RULE,   55}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,   55}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,   55}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,   55}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,   55}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   55}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   55}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   55}},
    {                 Token::IMPORT, {           TA_REDUCE_USING_RULE,   55}},

// ///////////////////////////////////////////////////////////////////////////
// state   15
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   42}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   62}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   63}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   42}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   42}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   42}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   42}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   42}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   42}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   42}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   42}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   42}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   42}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   42}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   42}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   42}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   42}},
    {                 Token::IMPORT, {           TA_REDUCE_USING_RULE,   42}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,   64}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   16
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   65}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   66}},

// ///////////////////////////////////////////////////////////////////////////
// state   17
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   67}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    // nonterminal transitions
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   68}},

// ///////////////////////////////////////////////////////////////////////////
// state   18
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                  Token::SCOPE, {        TA_SHIFT_AND_PUSH_STATE,   69}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state   19
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  105}},

// ///////////////////////////////////////////////////////////////////////////
// state   20
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  108}},

// ///////////////////////////////////////////////////////////////////////////
// state   21
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   70}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,   71}},

// ///////////////////////////////////////////////////////////////////////////
// state   22
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    // nonterminal transitions
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   72}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   73}},

// ///////////////////////////////////////////////////////////////////////////
// state   23
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   57}},

// ///////////////////////////////////////////////////////////////////////////
// state   24
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   58}},

// ///////////////////////////////////////////////////////////////////////////
// state   25
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   59}},

// ///////////////////////////////////////////////////////////////////////////
// state   26
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   60}},

// ///////////////////////////////////////////////////////////////////////////
// state   27
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   74}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   28
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   76}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   29
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   78}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   30
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   80}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   81}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   31
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {               Token::FUNCTION, {        TA_SHIFT_AND_PUSH_STATE,   82}},

// ///////////////////////////////////////////////////////////////////////////
// state   32
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    // nonterminal transitions
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   83}},

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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,   84}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state   34
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   85}},

// ///////////////////////////////////////////////////////////////////////////
// state   35
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   19}},

// ///////////////////////////////////////////////////////////////////////////
// state   36
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   15}},

// ///////////////////////////////////////////////////////////////////////////
// state   37
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,   86}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state   38
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   18}},

// ///////////////////////////////////////////////////////////////////////////
// state   39
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   61}},

// ///////////////////////////////////////////////////////////////////////////
// state   40
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,  115}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  107}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  108}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('-'), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('+'), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('*'), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,  115}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,  115}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,  115}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,  115}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,  115}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,  115}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,  115}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,  115}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,  115}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,  115}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,  115}},
    {                    Token::NOT, {           TA_REDUCE_USING_RULE,  115}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,  115}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,  115}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,  115}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,  115}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,  115}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,  115}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,  115}},
    {               Token::BAREWORD, {           TA_REDUCE_USING_RULE,  115}},
    {         Token::VAR_IDENTIFIER, {           TA_REDUCE_USING_RULE,  115}},
    {       Token::ARRAY_IDENTIFIER, {           TA_REDUCE_USING_RULE,  115}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,  115}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,  115}},
    {                    Token::INT, {           TA_REDUCE_USING_RULE,  115}},
    {                  Token::FLOAT, {           TA_REDUCE_USING_RULE,  115}},
    {                 Token::STRING, {           TA_REDUCE_USING_RULE,  115}},
    {                Token::BOOLEAN, {           TA_REDUCE_USING_RULE,  115}},
    {              Token::INCREMENT, {           TA_REDUCE_USING_RULE,  115}},
    {              Token::DECREMENT, {           TA_REDUCE_USING_RULE,  115}},
    {                    Token::CAT, {           TA_REDUCE_USING_RULE,  115}},
    {                    Token::POP, {           TA_REDUCE_USING_RULE,  115}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,  115}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,  115}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,  115}},
    {                 Token::IMPORT, {           TA_REDUCE_USING_RULE,  115}},

// ///////////////////////////////////////////////////////////////////////////
// state   41
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   62}},

// ///////////////////////////////////////////////////////////////////////////
// state   42
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   56}},

// ///////////////////////////////////////////////////////////////////////////
// state   43
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  109}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  110}},

// ///////////////////////////////////////////////////////////////////////////
// state   44
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   94}},

// ///////////////////////////////////////////////////////////////////////////
// state   45
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   99}},

// ///////////////////////////////////////////////////////////////////////////
// state   46
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   23}},

// ///////////////////////////////////////////////////////////////////////////
// state   47
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state   48
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  111}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state   49
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   21}},

// ///////////////////////////////////////////////////////////////////////////
// state   50
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  112}},
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,   36}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state   51
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   85}},

// ///////////////////////////////////////////////////////////////////////////
// state   52
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   86}},

// ///////////////////////////////////////////////////////////////////////////
// state   53
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   67}},

// ///////////////////////////////////////////////////////////////////////////
// state   54
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   87}},

// ///////////////////////////////////////////////////////////////////////////
// state   55
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   89}},

// ///////////////////////////////////////////////////////////////////////////
// state   56
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   27}},

// ///////////////////////////////////////////////////////////////////////////
// state   57
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   26}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  113}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   26}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   26}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   26}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   26}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   26}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   26}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   26}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   26}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   26}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   26}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   26}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   26}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   26}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   26}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   26}},
    {                 Token::IMPORT, {           TA_REDUCE_USING_RULE,   26}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  114}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   58
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   51}},

// ///////////////////////////////////////////////////////////////////////////
// state   59
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   50}},

// ///////////////////////////////////////////////////////////////////////////
// state   60
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   54}},

// ///////////////////////////////////////////////////////////////////////////
// state   61
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   53}},

// ///////////////////////////////////////////////////////////////////////////
// state   62
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   41}},

// ///////////////////////////////////////////////////////////////////////////
// state   63
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   40}},

// ///////////////////////////////////////////////////////////////////////////
// state   64
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  115}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state   65
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   38}},

// ///////////////////////////////////////////////////////////////////////////
// state   66
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  116}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  117}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   67
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  118}},

// ///////////////////////////////////////////////////////////////////////////
// state   68
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  119}},

// ///////////////////////////////////////////////////////////////////////////
// state   69
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,  120}},

// ///////////////////////////////////////////////////////////////////////////
// state   70
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   49}},

// ///////////////////////////////////////////////////////////////////////////
// state   71
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  121}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  122}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   72
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  123}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  116}},

// ///////////////////////////////////////////////////////////////////////////
// state   73
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  124}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,  125}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  120}},

// ///////////////////////////////////////////////////////////////////////////
// state   74
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   93}},

// ///////////////////////////////////////////////////////////////////////////
// state   75
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   92}},

// ///////////////////////////////////////////////////////////////////////////
// state   76
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   97}},

// ///////////////////////////////////////////////////////////////////////////
// state   77
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   96}},

// ///////////////////////////////////////////////////////////////////////////
// state   78
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   88}},

// ///////////////////////////////////////////////////////////////////////////
// state   79
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   90}},

// ///////////////////////////////////////////////////////////////////////////
// state   80
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  101}},

// ///////////////////////////////////////////////////////////////////////////
// state   81
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  100}},

// ///////////////////////////////////////////////////////////////////////////
// state   82
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  126}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    // nonterminal transitions
    {      Token::func_identifier__, {                  TA_PUSH_STATE,  127}},

// ///////////////////////////////////////////////////////////////////////////
// state   83
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,  128}},

// ///////////////////////////////////////////////////////////////////////////
// state   84
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  129}},
    {                  Token::WHILE, {        TA_SHIFT_AND_PUSH_STATE,  130}},

// ///////////////////////////////////////////////////////////////////////////
// state   85
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  131}},

// ///////////////////////////////////////////////////////////////////////////
// state   86
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  103}},

// ///////////////////////////////////////////////////////////////////////////
// state   87
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   88
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   89
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   65}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {           TA_REDUCE_USING_RULE,   65}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  134}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   90
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   64}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {           TA_REDUCE_USING_RULE,   64}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  135}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   91
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,   66}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,   66}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   66}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,   66}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,   66}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,   66}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,   66}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,   66}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   66}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {              Token::Type('/'), {           TA_REDUCE_USING_RULE,   66}},
    {              Token::Type('^'), {           TA_REDUCE_USING_RULE,   66}},
    {              Token::Type('%'), {           TA_REDUCE_USING_RULE,   66}},
    {                      Token::D, {           TA_REDUCE_USING_RULE,   66}},
    {                     Token::GT, {           TA_REDUCE_USING_RULE,   66}},
    {                     Token::LT, {           TA_REDUCE_USING_RULE,   66}},
    {                     Token::EQ, {           TA_REDUCE_USING_RULE,   66}},
    {                     Token::NE, {           TA_REDUCE_USING_RULE,   66}},
    {                    Token::GTE, {           TA_REDUCE_USING_RULE,   66}},
    {                    Token::LTE, {           TA_REDUCE_USING_RULE,   66}},
    {                    Token::AND, {           TA_REDUCE_USING_RULE,   66}},
    {                     Token::OR, {           TA_REDUCE_USING_RULE,   66}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,   66}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,   66}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,   66}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,   66}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,   66}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,   66}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,   66}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,   66}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,   66}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,   66}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,   66}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,   66}},
    {                 Token::IMPORT, {           TA_REDUCE_USING_RULE,   66}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  136}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   92
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   93
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   94
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   95
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   96
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   97
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   98
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state   99
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  100
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  145}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  101
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  146}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  102
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  147}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  103
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  148}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  104
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   95}},

// ///////////////////////////////////////////////////////////////////////////
// state  105
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   98}},

// ///////////////////////////////////////////////////////////////////////////
// state  106
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  149}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  107
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  150}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  114}},

// ///////////////////////////////////////////////////////////////////////////
// state  108
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::END_, {           TA_REDUCE_USING_RULE,  112}},
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type(';'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  151}},
    {              Token::Type('='), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('{'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('}'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('['), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type(']'), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,  112}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
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
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {                  Token::WHILE, {           TA_REDUCE_USING_RULE,  112}},
    {                  Token::BREAK, {           TA_REDUCE_USING_RULE,  112}},
    {               Token::CONTINUE, {           TA_REDUCE_USING_RULE,  112}},
    {                 Token::RETURN, {           TA_REDUCE_USING_RULE,  112}},
    {                     Token::IF, {           TA_REDUCE_USING_RULE,  112}},
    {                   Token::ELSE, {           TA_REDUCE_USING_RULE,  112}},
    {               Token::FUNCTION, {           TA_REDUCE_USING_RULE,  112}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
    {         Token::VAR_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   19}},
    {       Token::ARRAY_IDENTIFIER, {        TA_SHIFT_AND_PUSH_STATE,   20}},
    {                    Token::FOR, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::VAR, {           TA_REDUCE_USING_RULE,  112}},
    {                    Token::INT, {        TA_SHIFT_AND_PUSH_STATE,   23}},
    {                  Token::FLOAT, {        TA_SHIFT_AND_PUSH_STATE,   24}},
    {                 Token::STRING, {        TA_SHIFT_AND_PUSH_STATE,   25}},
    {                Token::BOOLEAN, {        TA_SHIFT_AND_PUSH_STATE,   26}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   27}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   28}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,   29}},
    {                    Token::POP, {        TA_SHIFT_AND_PUSH_STATE,   30}},
    {                  Token::FINAL, {           TA_REDUCE_USING_RULE,  112}},
    {               Token::CONSTANT, {           TA_REDUCE_USING_RULE,  112}},
    {                     Token::DO, {           TA_REDUCE_USING_RULE,  112}},
    {                 Token::IMPORT, {           TA_REDUCE_USING_RULE,  112}},
    // nonterminal transitions
    {                  Token::exp__, {                  TA_PUSH_STATE,  152}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {           Token::param_list__, {                  TA_PUSH_STATE,  153}},

// ///////////////////////////////////////////////////////////////////////////
// state  109
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,  154}},

// ///////////////////////////////////////////////////////////////////////////
// state  110
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   22}},

// ///////////////////////////////////////////////////////////////////////////
// state  111
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   84}},

// ///////////////////////////////////////////////////////////////////////////
// state  112
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   20}},

// ///////////////////////////////////////////////////////////////////////////
// state  113
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  155}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state  114
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  156}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state  115
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   39}},

// ///////////////////////////////////////////////////////////////////////////
// state  116
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  157}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   37}},

// ///////////////////////////////////////////////////////////////////////////
// state  117
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  158}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state  118
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  159}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  160}},

// ///////////////////////////////////////////////////////////////////////////
// state  119
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  161}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   10}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   10}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  162}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  160}},

// ///////////////////////////////////////////////////////////////////////////
// state  120
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  107}},

// ///////////////////////////////////////////////////////////////////////////
// state  121
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   45}},

// ///////////////////////////////////////////////////////////////////////////
// state  122
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  163}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,  164}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  123
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  124
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  166}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  125
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  167}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  126
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  168}},

// ///////////////////////////////////////////////////////////////////////////
// state  127
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  169}},

// ///////////////////////////////////////////////////////////////////////////
// state  128
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  170}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  129
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   31}},

// ///////////////////////////////////////////////////////////////////////////
// state  130
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,  171}},

// ///////////////////////////////////////////////////////////////////////////
// state  131
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   17}},

// ///////////////////////////////////////////////////////////////////////////
// state  132
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   73}},

// ///////////////////////////////////////////////////////////////////////////
// state  133
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  172}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state  134
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   68}},

// ///////////////////////////////////////////////////////////////////////////
// state  135
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   63}},

// ///////////////////////////////////////////////////////////////////////////
// state  136
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   69}},

// ///////////////////////////////////////////////////////////////////////////
// state  137
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   70}},

// ///////////////////////////////////////////////////////////////////////////
// state  138
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   74}},

// ///////////////////////////////////////////////////////////////////////////
// state  139
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   71}},

// ///////////////////////////////////////////////////////////////////////////
// state  140
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   72}},

// ///////////////////////////////////////////////////////////////////////////
// state  141
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   80}},

// ///////////////////////////////////////////////////////////////////////////
// state  142
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   79}},

// ///////////////////////////////////////////////////////////////////////////
// state  143
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   77}},

// ///////////////////////////////////////////////////////////////////////////
// state  144
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   78}},

// ///////////////////////////////////////////////////////////////////////////
// state  145
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   82}},

// ///////////////////////////////////////////////////////////////////////////
// state  146
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   81}},

// ///////////////////////////////////////////////////////////////////////////
// state  147
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   76}},

// ///////////////////////////////////////////////////////////////////////////
// state  148
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   75}},

// ///////////////////////////////////////////////////////////////////////////
// state  149
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   83}},

// ///////////////////////////////////////////////////////////////////////////
// state  150
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  113}},

// ///////////////////////////////////////////////////////////////////////////
// state  151
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  109}},

// ///////////////////////////////////////////////////////////////////////////
// state  152
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  122}},

// ///////////////////////////////////////////////////////////////////////////
// state  153
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  173}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  174}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  111}},

// ///////////////////////////////////////////////////////////////////////////
// state  154
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   24}},

// ///////////////////////////////////////////////////////////////////////////
// state  155
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,  175}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  156
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,  176}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  157
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,  177}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  158
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,  178}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  159
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  179}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  180}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  181}},

// ///////////////////////////////////////////////////////////////////////////
// state  160
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   11}},

// ///////////////////////////////////////////////////////////////////////////
// state  161
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  182}},

// ///////////////////////////////////////////////////////////////////////////
// state  162
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  179}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  183}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  181}},

// ///////////////////////////////////////////////////////////////////////////
// state  163
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   46}},

// ///////////////////////////////////////////////////////////////////////////
// state  164
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  184}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  185}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  186}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  165
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  117}},

// ///////////////////////////////////////////////////////////////////////////
// state  166
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  121}},

// ///////////////////////////////////////////////////////////////////////////
// state  167
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type(']'), {        TA_SHIFT_AND_PUSH_STATE,  187}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state  168
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   10}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  188}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  160}},

// ///////////////////////////////////////////////////////////////////////////
// state  169
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  189}},
    {              Token::Type(')'), {           TA_REDUCE_USING_RULE,   10}},
    {              Token::Type(','), {           TA_REDUCE_USING_RULE,   10}},
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {     Token::param_definition__, {                  TA_PUSH_STATE,  190}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,  160}},

// ///////////////////////////////////////////////////////////////////////////
// state  170
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  118}},

// ///////////////////////////////////////////////////////////////////////////
// state  171
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  191}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  192}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  172
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   91}},

// ///////////////////////////////////////////////////////////////////////////
// state  173
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  110}},

// ///////////////////////////////////////////////////////////////////////////
// state  174
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,   47}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,    8}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,    9}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   10}},
    {                    Token::NOT, {        TA_SHIFT_AND_PUSH_STATE,   11}},
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                  Token::exp__, {                  TA_PUSH_STATE,  193}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},

// ///////////////////////////////////////////////////////////////////////////
// state  175
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   28}},

// ///////////////////////////////////////////////////////////////////////////
// state  176
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   25}},

// ///////////////////////////////////////////////////////////////////////////
// state  177
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  194}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   36}},

// ///////////////////////////////////////////////////////////////////////////
// state  178
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                   Token::ELSE, {        TA_SHIFT_AND_PUSH_STATE,  195}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   34}},

// ///////////////////////////////////////////////////////////////////////////
// state  179
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   13}},

// ///////////////////////////////////////////////////////////////////////////
// state  180
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  196}},

// ///////////////////////////////////////////////////////////////////////////
// state  181
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                    Token::VAR, {        TA_SHIFT_AND_PUSH_STATE,   22}},
    {               Token::CONSTANT, {        TA_SHIFT_AND_PUSH_STATE,   32}},
    // nonterminal transitions
    {              Token::vardecl__, {                  TA_PUSH_STATE,  197}},

// ///////////////////////////////////////////////////////////////////////////
// state  182
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  198}},

// ///////////////////////////////////////////////////////////////////////////
// state  183
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  199}},

// ///////////////////////////////////////////////////////////////////////////
// state  184
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   47}},

// ///////////////////////////////////////////////////////////////////////////
// state  185
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,  200}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  186
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  201}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   48}},

// ///////////////////////////////////////////////////////////////////////////
// state  187
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  119}},

// ///////////////////////////////////////////////////////////////////////////
// state  188
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  179}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  202}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  181}},

// ///////////////////////////////////////////////////////////////////////////
// state  189
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  203}},

// ///////////////////////////////////////////////////////////////////////////
// state  190
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,  179}},
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  204}},
    {              Token::Type(','), {        TA_SHIFT_AND_PUSH_STATE,  181}},

// ///////////////////////////////////////////////////////////////////////////
// state  191
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  205}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,   44}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,   45}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   32}},

// ///////////////////////////////////////////////////////////////////////////
// state  192
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type(')'), {        TA_SHIFT_AND_PUSH_STATE,  206}},
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},

// ///////////////////////////////////////////////////////////////////////////
// state  193
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('='), {        TA_SHIFT_AND_PUSH_STATE,   87}},
    {              Token::Type('['), {        TA_SHIFT_AND_PUSH_STATE,   88}},
    {              Token::Type('-'), {        TA_SHIFT_AND_PUSH_STATE,   89}},
    {              Token::Type('+'), {        TA_SHIFT_AND_PUSH_STATE,   90}},
    {              Token::Type('*'), {        TA_SHIFT_AND_PUSH_STATE,   91}},
    {              Token::Type('/'), {        TA_SHIFT_AND_PUSH_STATE,   92}},
    {              Token::Type('^'), {        TA_SHIFT_AND_PUSH_STATE,   93}},
    {              Token::Type('%'), {        TA_SHIFT_AND_PUSH_STATE,   94}},
    {                      Token::D, {        TA_SHIFT_AND_PUSH_STATE,   95}},
    {                     Token::GT, {        TA_SHIFT_AND_PUSH_STATE,   96}},
    {                     Token::LT, {        TA_SHIFT_AND_PUSH_STATE,   97}},
    {                     Token::EQ, {        TA_SHIFT_AND_PUSH_STATE,   98}},
    {                     Token::NE, {        TA_SHIFT_AND_PUSH_STATE,   99}},
    {                    Token::GTE, {        TA_SHIFT_AND_PUSH_STATE,  100}},
    {                    Token::LTE, {        TA_SHIFT_AND_PUSH_STATE,  101}},
    {                    Token::AND, {        TA_SHIFT_AND_PUSH_STATE,  102}},
    {                     Token::OR, {        TA_SHIFT_AND_PUSH_STATE,  103}},
    {              Token::INCREMENT, {        TA_SHIFT_AND_PUSH_STATE,  104}},
    {              Token::DECREMENT, {        TA_SHIFT_AND_PUSH_STATE,  105}},
    {                    Token::CAT, {        TA_SHIFT_AND_PUSH_STATE,  106}},
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,  123}},

// ///////////////////////////////////////////////////////////////////////////
// state  194
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,  207}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,  208}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  196
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  209}},

// ///////////////////////////////////////////////////////////////////////////
// state  197
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   12}},

// ///////////////////////////////////////////////////////////////////////////
// state  198
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  210}},

// ///////////////////////////////////////////////////////////////////////////
// state  199
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  211}},

// ///////////////////////////////////////////////////////////////////////////
// state  200
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  201
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,  212}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  202
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  213}},

// ///////////////////////////////////////////////////////////////////////////
// state  203
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  214}},

// ///////////////////////////////////////////////////////////////////////////
// state  204
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,  215}},

// ///////////////////////////////////////////////////////////////////////////
// state  205
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   30}},

// ///////////////////////////////////////////////////////////////////////////
// state  206
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   29}},

// ///////////////////////////////////////////////////////////////////////////
// state  207
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   35}},

// ///////////////////////////////////////////////////////////////////////////
// state  208
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   33}},

// ///////////////////////////////////////////////////////////////////////////
// state  209
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,   36}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  210
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,   36}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  211
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,   36}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  212
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   44}},

// ///////////////////////////////////////////////////////////////////////////
// state  213
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  219}},

// ///////////////////////////////////////////////////////////////////////////
// state  214
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  220}},

// ///////////////////////////////////////////////////////////////////////////
// state  215
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,   14}},
    // nonterminal transitions
    {       Token::statement_list__, {                  TA_PUSH_STATE,  221}},

// ///////////////////////////////////////////////////////////////////////////
// state  216
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    5}},

// ///////////////////////////////////////////////////////////////////////////
// state  217
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    3}},

// ///////////////////////////////////////////////////////////////////////////
// state  218
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    2}},

// ///////////////////////////////////////////////////////////////////////////
// state  219
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  222}},
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,   36}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  220
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  223}},
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,   36}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  221
// ///////////////////////////////////////////////////////////////////////////
    // terminal transitions
    {                 Token::ERROR_, {        TA_SHIFT_AND_PUSH_STATE,    4}},
    {              Token::Type(';'), {        TA_SHIFT_AND_PUSH_STATE,    5}},
    {              Token::Type('('), {        TA_SHIFT_AND_PUSH_STATE,    6}},
    {              Token::Type('{'), {        TA_SHIFT_AND_PUSH_STATE,    7}},
    {              Token::Type('}'), {        TA_SHIFT_AND_PUSH_STATE,  224}},
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
    {               Token::BAREWORD, {        TA_SHIFT_AND_PUSH_STATE,   18}},
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
    {                 Token::IMPORT, {        TA_SHIFT_AND_PUSH_STATE,   34}},
    // nonterminal transitions
    {      Token::func_definition__, {                  TA_PUSH_STATE,   35}},
    {            Token::statement__, {                  TA_PUSH_STATE,   36}},
    {                  Token::exp__, {                  TA_PUSH_STATE,   37}},
    {        Token::exp_statement__, {                  TA_PUSH_STATE,   38}},
    {       Token::var_identifier__, {                  TA_PUSH_STATE,   39}},
    {      Token::func_identifier__, {                  TA_PUSH_STATE,   40}},
    {     Token::array_identifier__, {                  TA_PUSH_STATE,   41}},
    {                 Token::call__, {                  TA_PUSH_STATE,   42}},
    {              Token::vardecl__, {                  TA_PUSH_STATE,   43}},

// ///////////////////////////////////////////////////////////////////////////
// state  222
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    6}},

// ///////////////////////////////////////////////////////////////////////////
// state  223
// ///////////////////////////////////////////////////////////////////////////
    // default transition
    {               Token::DEFAULT_, {           TA_REDUCE_USING_RULE,    4}},

// ///////////////////////////////////////////////////////////////////////////
// state  224
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

#line 6447 "SteelParser.cpp"

