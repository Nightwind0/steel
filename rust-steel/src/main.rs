use std::cell::RefCell;
use std::collections::HashMap;
use std::env;
use std::fs;
use std::io::{self, Read, Write};
use std::rc::Rc;

type EnvRef = Rc<Env>;
type Cell = Rc<RefCell<Value>>;

#[derive(Clone)]
enum Value {
    Null,
    Bool(bool),
    Int(i64),
    Real(f64),
    Str(String),
    Array(Rc<RefCell<Vec<Value>>>),
    Hash(Rc<RefCell<HashMap<String, Value>>>),
    Function(Rc<Function>),
    Native(String),
}

impl Value {
    fn kind(&self) -> &'static str {
        match self { Self::Null => "null", Self::Bool(_) => "boolean", Self::Int(_) => "integer", Self::Real(_) => "real", Self::Str(_) => "string", Self::Array(_) => "array", Self::Hash(_) => "hash", Self::Function(_) | Self::Native(_) => "function" }
    }

    fn truthy(&self) -> bool {
        match self {
            Self::Null => false,
            Self::Bool(v) => *v,
            Self::Int(v) => *v != 0,
            Self::Real(v) => *v != 0.0,
            Self::Str(v) => !v.is_empty(),
            Self::Array(_) | Self::Hash(_) | Self::Function(_) => true,
            Self::Native(_) => false,
        }
    }

    fn number(&self) -> Result<f64, String> {
        match self {
            Self::Bool(v) => Ok(if *v { 1.0 } else { 0.0 }),
            Self::Int(v) => Ok(*v as f64),
            Self::Real(v) => Ok(*v),
            Self::Str(v) => v.parse::<f64>().map_err(|_| format!("cannot convert '{v}' to a number")),
            _ => Err("value is not numeric".to_owned()),
        }
    }

    fn integer(&self) -> Result<i64, String> {
        Ok(self.number()? as i64)
    }

    fn string(&self) -> String {
        match self {
            Self::Null => "0".to_owned(),
            Self::Bool(v) => if *v { "TRUE" } else { "FALSE" }.to_owned(),
            Self::Int(v) => v.to_string(),
            Self::Real(v) => {
                if v.fract() == 0.0 { format!("{v:.1}") } else { v.to_string() }
            }
            Self::Str(v) => v.clone(),
            Self::Array(values) => format!("#A({:p})", Rc::as_ptr(values)),
            Self::Hash(values) => format!("#M({:p})", Rc::as_ptr(values)),
            Self::Function(v) => format!("#F({:p})", Rc::as_ptr(v)),
            Self::Native(name) => format!("#F({name})"),
        }
    }

}

struct Env {
    parent: Option<EnvRef>,
    values: RefCell<HashMap<String, Cell>>,
}

impl Env {
    fn new(parent: Option<EnvRef>) -> EnvRef {
        Rc::new(Self { parent, values: RefCell::new(HashMap::new()) })
    }
    fn define(&self, name: String, value: Value) -> Cell {
        let cell = Rc::new(RefCell::new(value));
        self.values.borrow_mut().insert(name, cell.clone());
        cell
    }
    fn get_cell(&self, name: &str) -> Option<Cell> {
        self.values.borrow().get(name).cloned().or_else(|| self.parent.as_ref()?.get_cell(name))
    }
}

#[derive(Clone)]
struct Function {
    params: Vec<Param>,
    body: Stmt,
    closure: EnvRef,
}

#[derive(Clone)]
struct Param { name: String, default: Option<Expr> }

#[derive(Clone)]
enum Expr {
    Literal(Value),
    Variable(String),
    Array(Vec<Expr>),
    Hash(Vec<(Expr, Expr)>),
    Unary(String, Box<Expr>),
    Binary(Box<Expr>, String, Box<Expr>),
    Assign(Box<Expr>, String, Box<Expr>),
    Index(Box<Expr>, Box<Expr>),
    Call(Box<Expr>, Vec<Expr>),
    Function(Vec<Param>, Box<Stmt>),
    Postfix(Box<Expr>, String),
    Sequence(Box<Expr>, Box<Expr>),
}

#[derive(Clone)]
enum Stmt {
    Empty,
    Expr(Expr),
    Block(Vec<Stmt>),
    Declare(String, Option<Expr>, Option<Expr>, bool),
    Function(String, Vec<Param>, Box<Stmt>),
    If(Expr, Box<Stmt>, Option<Box<Stmt>>),
    While(Expr, Box<Stmt>),
    DoWhile(Box<Stmt>, Expr),
    For(Option<Box<Stmt>>, Option<Expr>, Option<Expr>, Box<Stmt>),
    ForEach(String, Expr, Box<Stmt>),
    Switch(Expr, Vec<(Expr, Vec<Stmt>)>, Option<Vec<Stmt>>),
    Import(String),
    Return(Option<Expr>),
    Break,
    Continue,
}

#[derive(Clone, Debug, PartialEq)]
enum TokenKind { Number(f64), String(String), Ident(String), Symbol(String), Eof }

#[derive(Clone, Debug)]
struct Token { kind: TokenKind, line: usize, column: usize }

struct Lexer<'a> { source: &'a str, bytes: &'a [u8], pos: usize, line: usize, col: usize }

impl<'a> Lexer<'a> {
    fn new(source: &'a str) -> Self { Self { source, bytes: source.as_bytes(), pos: 0, line: 1, col: 1 } }
    fn peek(&self, n: usize) -> Option<u8> { self.bytes.get(self.pos + n).copied() }
    fn advance(&mut self) -> Option<u8> {
        let ch = self.peek(0)?;
        self.pos += 1;
        if ch == b'\n' { self.line += 1; self.col = 1; } else { self.col += 1; }
        Some(ch)
    }
    fn tokens(mut self) -> Result<Vec<Token>, String> {
        let mut out = Vec::new();
        while let Some(ch) = self.peek(0) {
            if ch.is_ascii_whitespace() { self.advance(); continue; }
            if ch == b'/' && self.peek(1) == Some(b'/') {
                while self.advance().is_some_and(|c| c != b'\n') {}
                continue;
            }
            if ch == b'/' && self.peek(1) == Some(b'*') {
                self.advance(); self.advance();
                let start = self.line;
                while !(self.peek(0) == Some(b'*') && self.peek(1) == Some(b'/')) {
                    if self.advance().is_none() { return Err(format!("{start}: unterminated block comment")); }
                }
                self.advance(); self.advance(); continue;
            }
            let line = self.line; let column = self.col;
            let kind = if ch == b'"' {
                self.advance();
                let mut value = String::new();
                loop {
                    match self.advance() {
                        Some(b'"') => break,
                        Some(b'\\') => match self.advance() {
                            Some(b'n') => value.push('\n'), Some(b'r') => value.push('\r'), Some(b't') => value.push('\t'),
                            Some(b'0') => value.push('\0'), Some(b'{') => value.push('\u{e000}'), Some(b'}') => value.push('\u{e001}'), Some(c) => value.push(c as char), None => return Err(format!("{line}: unterminated escape")),
                        },
                        Some(c) => value.push(c as char),
                        None => return Err(format!("{line}: unterminated string")),
                    }
                }
                TokenKind::String(value)
            } else if ch.is_ascii_digit() {
                let start = self.pos;
                if ch == b'0' && matches!(self.peek(1), Some(b'x' | b'X')) {
                    self.advance(); self.advance();
                    while self.peek(0).is_some_and(|c| c.is_ascii_hexdigit()) { self.advance(); }
                    let hex = &self.source[start + 2..self.pos];
                    TokenKind::Number(i64::from_str_radix(hex, 16).map_err(|e| e.to_string())? as f64)
                } else {
                    self.advance();
                    while self.peek(0).is_some_and(|c| c.is_ascii_digit()) { self.advance(); }
                    if self.peek(0) == Some(b'.') && self.peek(1).is_some_and(|c| c.is_ascii_digit()) {
                        self.advance(); while self.peek(0).is_some_and(|c| c.is_ascii_digit()) { self.advance(); }
                    }
                    if matches!(self.peek(0), Some(b'e' | b'E')) {
                        self.advance(); if matches!(self.peek(0), Some(b'+' | b'-')) { self.advance(); }
                        while self.peek(0).is_some_and(|c| c.is_ascii_digit()) { self.advance(); }
                    }
                    TokenKind::Number(self.source[start..self.pos].parse().map_err(|e: std::num::ParseFloatError| e.to_string())?)
                }
            } else if ch.is_ascii_alphabetic() || ch == b'_' || matches!(ch, b'$' | b'@' | b'#') {
                let start = self.pos; self.advance();
                while self.peek(0).is_some_and(|c| c.is_ascii_alphanumeric() || c == b'_') { self.advance(); }
                TokenKind::Ident(self.source[start..self.pos].to_owned())
            } else {
                let two = self.source.get(self.pos..self.pos + 2).unwrap_or("");
                let three = self.source.get(self.pos..self.pos + 3).unwrap_or("");
                let operator = if ["<<=", ">>=", "++=", "--="].contains(&three) { self.advance(); self.advance(); self.advance(); three }
                    else if ["==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=", "-=", "*=", "/=", "%=", "::", "=>", "<<", ">>"].contains(&two) { self.advance(); self.advance(); two }
                    else if b"{}[]();,:+-*/%^=<>!~&|".contains(&ch) { self.advance(); &self.source[self.pos - 1..self.pos] }
                    else { return Err(format!("{line}:{column}: unexpected character {:?}", ch as char)); };
                TokenKind::Symbol(operator.to_owned())
            };
            out.push(Token { kind, line, column });
        }
        out.push(Token { kind: TokenKind::Eof, line: self.line, column: self.col });
        Ok(out)
    }
}

struct Parser { tokens: Vec<Token>, pos: usize }

impl Parser {
    fn new(tokens: Vec<Token>) -> Self { Self { tokens, pos: 0 } }
    fn token(&self) -> &Token { &self.tokens[self.pos] }
    fn symbol(&self, value: &str) -> bool { self.token().kind == TokenKind::Symbol(value.to_owned()) }
    fn keyword(&self, value: &str) -> bool { self.token().kind == TokenKind::Ident(value.to_owned()) }
    fn advance(&mut self) -> Token { let token = self.tokens[self.pos].clone(); if !matches!(token.kind, TokenKind::Eof) { self.pos += 1; } token }
    fn error<T>(&self, message: &str) -> Result<T, String> { Err(format!("{}:{}: {message}", self.token().line, self.token().column)) }
    fn eat_symbol(&mut self, value: &str) -> bool { if self.symbol(value) { self.advance(); true } else { false } }
    fn expect_symbol(&mut self, value: &str) -> Result<(), String> { if self.eat_symbol(value) { Ok(()) } else { self.error(&format!("expected '{value}'")) } }
    fn eat_keyword(&mut self, value: &str) -> bool { if self.keyword(value) { self.advance(); true } else { false } }
    fn identifier(&mut self) -> Result<String, String> { match self.advance().kind { TokenKind::Ident(s) => Ok(s), _ => self.error("expected an identifier") } }
    fn program(&mut self) -> Result<Stmt, String> { let mut body = Vec::new(); while !matches!(self.token().kind, TokenKind::Eof) { body.push(self.statement()?); } Ok(Stmt::Block(body)) }

    fn statement(&mut self) -> Result<Stmt, String> {
        if self.eat_symbol(";") { return Ok(Stmt::Empty); }
        if self.symbol("{") { return self.block(); }
        if self.keyword("function") && self.tokens.get(self.pos + 1).is_some_and(|token| token.kind != TokenKind::Symbol("(".into())) {
            self.advance();
            let mut name = self.identifier()?;
            if self.eat_symbol("::") { name.push_str("::"); name.push_str(&self.identifier()?); }
            let params = self.params()?; let body = Box::new(self.block()?);
            return Ok(Stmt::Function(name, params, body));
        }
        if self.keyword("var") || self.keyword("declare") || self.keyword("const") { return self.declaration(true); }
        if self.eat_keyword("if") {
            self.expect_symbol("(")?; let cond = self.expression(0)?; self.expect_symbol(")")?;
            let yes = Box::new(self.statement()?);
            let no = if self.eat_keyword("else") { Some(Box::new(self.statement()?)) } else { None };
            return Ok(Stmt::If(cond, yes, no));
        }
        if self.eat_keyword("while") { self.expect_symbol("(")?; let cond = self.expression(0)?; self.expect_symbol(")")?; return Ok(Stmt::While(cond, Box::new(self.statement()?))); }
        if self.eat_keyword("do") { let body = Box::new(self.statement()?); self.eat_keyword("while"); self.expect_symbol("(")?; let cond = self.expression(0)?; self.expect_symbol(")")?; self.eat_symbol(";"); return Ok(Stmt::DoWhile(body, cond)); }
        if self.eat_keyword("for") {
            self.expect_symbol("(")?;
            let init = if self.symbol(";") { None } else if self.keyword("var") || self.keyword("declare") || self.keyword("const") { Some(Box::new(self.declaration(false)?)) } else { Some(Box::new(Stmt::Expr(self.expression(0)?))) };
            self.expect_symbol(";")?;
            let cond = if self.symbol(";") { None } else { Some(self.expression(0)?) };
            self.expect_symbol(";")?;
            let iter = if self.symbol(")") { None } else { Some(self.expression(0)?) };
            self.expect_symbol(")")?;
            return Ok(Stmt::For(init, cond, iter, Box::new(self.statement()?)));
        }
        if self.eat_keyword("foreach") {
            let wrapped = self.eat_symbol("(");
            self.eat_keyword("declare"); self.eat_keyword("var");
            let name = self.identifier()?;
            if wrapped { self.expect_symbol(")")?; }
            if !self.eat_keyword("in") && !self.eat_keyword("within") { return self.error("expected 'in' after foreach variable"); }
            let source = self.expression(0)?; let body = Box::new(self.statement()?);
            return Ok(Stmt::ForEach(name, source, body));
        }
        if self.eat_keyword("return") { let value = if self.symbol(";") { None } else { Some(self.expression(0)?) }; self.expect_symbol(";")?; return Ok(Stmt::Return(value)); }
        if self.eat_keyword("break") { self.eat_symbol(";"); return Ok(Stmt::Break); }
        if self.eat_keyword("continue") { self.eat_symbol(";"); return Ok(Stmt::Continue); }
        if self.eat_keyword("using") || self.eat_keyword("import") {
            let namespace = match self.advance().kind { TokenKind::String(name) => name, _ => return self.error("expected a namespace string") };
            self.eat_symbol(";");
            return Ok(Stmt::Import(namespace));
        }
        if self.eat_keyword("switch") {
            self.expect_symbol("(")?;
            let value = self.expression(0)?;
            self.expect_symbol(")")?;
            self.expect_symbol("{")?;
            let mut cases = Vec::new();
            let mut default = None;
            while !self.symbol("}") && !matches!(self.token().kind, TokenKind::Eof) {
                if self.eat_keyword("case") {
                    let label = self.expression(2)?;
                    self.expect_symbol(":")?;
                    let mut body = Vec::new();
                    while !self.symbol("}") && !self.keyword("case") && !self.keyword("default") && !matches!(self.token().kind, TokenKind::Eof) { body.push(self.statement()?); }
                    cases.push((label, body));
                } else if self.eat_keyword("default") {
                    self.expect_symbol(":")?;
                    let mut body = Vec::new();
                    while !self.symbol("}") && !self.keyword("case") && !self.keyword("default") && !matches!(self.token().kind, TokenKind::Eof) { body.push(self.statement()?); }
                    if default.replace(body).is_some() { return self.error("duplicate default label"); }
                } else { return self.error("expected case or default label in switch"); }
            }
            self.expect_symbol("}")?;
            return Ok(Stmt::Switch(value, cases, default));
        }
        if self.eat_keyword("include") { return self.error("include is not implemented in the Rust interpreter yet"); }
        let expr = self.expression(0)?; self.expect_symbol(";")?; Ok(Stmt::Expr(expr))
    }

    fn block(&mut self) -> Result<Stmt, String> {
        self.expect_symbol("{")?; let mut body = Vec::new();
        while !self.symbol("}") && !matches!(self.token().kind, TokenKind::Eof) { body.push(self.statement()?); }
        self.expect_symbol("}")?; Ok(Stmt::Block(body))
    }

    fn declaration(&mut self, semicolon: bool) -> Result<Stmt, String> {
        let constant = self.eat_keyword("const");
        if !constant { self.eat_keyword("var"); self.eat_keyword("declare"); }
        let name = self.identifier()?;
        let size = if self.eat_symbol("[") { let n = self.expression(0)?; self.expect_symbol("]")?; Some(n) } else { None };
        let init = if self.eat_symbol("=") { Some(self.expression(0)?) } else { None };
        if semicolon { self.expect_symbol(";")?; }
        Ok(Stmt::Declare(name, size, init, constant))
    }

    fn params(&mut self) -> Result<Vec<Param>, String> {
        self.expect_symbol("(")?; let mut params = Vec::new();
        if !self.symbol(")") { loop {
            self.eat_keyword("declare"); self.eat_keyword("var");
            let name = self.identifier()?;
            let default = if self.eat_symbol("=") { Some(self.expression(2)?) } else { None };
            params.push(Param { name, default });
            if !self.eat_symbol(",") { break; }
        }}
        self.expect_symbol(")")?; Ok(params)
    }

    fn expression(&mut self, min_prec: u8) -> Result<Expr, String> {
        let mut left = self.prefix()?;
        loop {
            if self.symbol("(") && 15 >= min_prec {
                self.advance(); let args = self.arguments(")")?; left = Expr::Call(Box::new(left), args); continue;
            }
            if self.eat_symbol("[") && 15 >= min_prec {
                let index = self.expression(0)?; self.expect_symbol("]")?; left = Expr::Index(Box::new(left), Box::new(index)); continue;
            }
            if (self.symbol("++") || self.symbol("--")) && 14 >= min_prec { let op = self.advance().kind; let op = if op == TokenKind::Symbol("++".into()) { "++" } else { "--" }; left = Expr::Postfix(Box::new(left), op.to_owned()); continue; }
            let op = match &self.token().kind { TokenKind::Symbol(op) => op.clone(), TokenKind::Ident(op) if op == "and" || op == "or" || op == "d" => op.clone(), _ => break };
            let (prec, right_assoc) = match op.as_str() {
                "," => (1, false), "=" | "+=" | "-=" | "*=" | "/=" | "%=" => (2, true),
                "or" | "||" => (3, false), "and" | "&&" => (4, false), "|" => (5, false), "&" => (6, false),
                "==" | "!=" => (7, false), "<" | "<=" | ">" | ">=" => (8, false),
                "+" | "-" => (9, false), "*" | "/" | "%" => (10, false), "^" | "d" => (11, true), _ => break,
            };
            if prec < min_prec { break; }
            self.advance(); let right = self.expression(if right_assoc { prec } else { prec + 1 })?;
            left = if op == "," { Expr::Sequence(Box::new(left), Box::new(right)) }
                else if prec == 2 { Expr::Assign(Box::new(left), op, Box::new(right)) }
                else { Expr::Binary(Box::new(left), op, Box::new(right)) };
        }
        Ok(left)
    }

    fn prefix(&mut self) -> Result<Expr, String> {
        if self.symbol("-") || self.symbol("+") || self.symbol("!") || self.symbol("~") || self.keyword("not") || self.symbol("++") || self.symbol("--") {
            let op = match self.advance().kind { TokenKind::Symbol(op) | TokenKind::Ident(op) => op, _ => unreachable!() };
            return Ok(Expr::Unary(op, Box::new(self.expression(12)?)));
        }
        if self.eat_keyword("pop") || self.eat_keyword("popb") {
            let pop_back = self.tokens[self.pos - 1].kind == TokenKind::Ident("popb".into());
            let expr = self.expression(12)?;
            return Ok(Expr::Call(Box::new(Expr::Variable(if pop_back { "__popb" } else { "__pop" }.to_owned())), vec![expr]));
        }
        if self.eat_keyword("push") || self.eat_keyword("pushb") {
            let push_front = self.tokens[self.pos - 1].kind == TokenKind::Ident("push".into());
            let target = self.expression(12)?; self.expect_symbol(",")?; let value = self.expression(2)?;
            return Ok(Expr::Call(Box::new(Expr::Variable(if push_front { "__push" } else { "__pushb" }.into())), vec![target, value]));
        }
        if self.eat_keyword("remove") { self.expect_symbol("(")?; let a = self.expression(2)?; self.expect_symbol(",")?; let b = self.expression(2)?; self.expect_symbol(")")?; return Ok(Expr::Call(Box::new(Expr::Variable("__remove".into())), vec![a,b])); }
        match self.advance().kind {
            TokenKind::Number(v) => Ok(Expr::Literal(if v.fract() == 0.0 { Value::Int(v as i64) } else { Value::Real(v) })),
            TokenKind::String(v) => Ok(Expr::Literal(Value::Str(v))),
            TokenKind::Ident(name) if name == "true" => Ok(Expr::Literal(Value::Bool(true))),
            TokenKind::Ident(name) if name == "false" => Ok(Expr::Literal(Value::Bool(false))),
            TokenKind::Ident(name) if name == "function" => { let params = self.params()?; let body = Box::new(self.block()?); Ok(Expr::Function(params, body)) },
            TokenKind::Ident(mut name) => {
                while self.eat_symbol("::") { name.push_str("::"); name.push_str(&self.identifier()?); }
                Ok(Expr::Variable(name))
            }
            TokenKind::Symbol(sym) if sym == "(" => { let expr = self.expression(0)?; self.expect_symbol(")")?; Ok(expr) }
            TokenKind::Symbol(sym) if sym == "[" => self.array_or_hash(),
            TokenKind::Symbol(sym) if sym == "{" => {
                self.pos -= 1; let stmt = self.block()?;
                match stmt { Stmt::Block(v) => Ok(Expr::Literal(Value::Array(Rc::new(RefCell::new(v.into_iter().map(|_| Value::Null).collect()))))), _ => unreachable!() }
            }
            _ => self.error("expected an expression"),
        }
    }

    fn array_or_hash(&mut self) -> Result<Expr, String> {
        if self.eat_symbol("]") { return Ok(Expr::Array(Vec::new())); }
        let first = self.expression(2)?;
        if self.eat_symbol("=>") {
            let mut pairs = vec![(first, self.expression(2)?)];
            while self.eat_symbol(",") { if self.symbol("]") { break; } let key = self.expression(2)?; self.expect_symbol("=>")?; pairs.push((key, self.expression(2)?)); }
            self.expect_symbol("]")?; Ok(Expr::Hash(pairs))
        } else {
            let mut values = vec![first];
            while self.eat_symbol(",") { if self.symbol("]") { break; } values.push(self.expression(2)?); }
            self.expect_symbol("]")?; Ok(Expr::Array(values))
        }
    }

    fn arguments(&mut self, end: &str) -> Result<Vec<Expr>, String> {
        let mut args = Vec::new();
        if !self.symbol(end) { loop { args.push(self.expression(2)?); if !self.eat_symbol(",") { break; } } }
        self.expect_symbol(end)?; Ok(args)
    }
}

enum Flow { Normal, Return(Value), Break, Continue }

struct Runtime { globals: EnvRef, script_dir: String, imports: Vec<String> }

impl Runtime {
    fn new(script_dir: String) -> Self {
        let globals = Env::new(None);
        for name in ["print", "println", "len", "real", "integer", "boolean", "string", "substr", "strlen", "is_array", "is_function", "is_handle", "is_valid", "array", "hash", "ceil", "abs", "floor", "exp", "log", "log10", "sqrt", "acos", "asin", "atan", "atan2", "cos", "sin", "tan", "cosh", "sinh", "tanh", "round", "pow", "rand", "randf", "srand", "rad2deg", "deg2rad", "require", "__pop", "__popb", "__push", "__pushb", "__remove"] { globals.define(name.to_owned(), Value::Native(name.to_owned())); }
        for name in ["ceil", "abs", "floor", "exp", "log", "log10", "sqrt", "acos", "asin", "atan", "atan2", "cos", "sin", "tan", "cosh", "sinh", "tanh", "round", "pow", "rand", "randf", "srand", "rad2deg", "deg2rad"] {
            let qualified = format!("math::{name}");
            globals.define(qualified.clone(), Value::Native(qualified));
        }
        Self { globals, script_dir, imports: Vec::new() }
    }
    fn run(&mut self, stmt: &Stmt) -> Result<Value, String> {
        match self.exec_program(stmt, self.globals.clone())? { Flow::Return(value) => Ok(value), _ => Ok(Value::Null) }
    }
    fn exec_program(&mut self, stmt: &Stmt, env: EnvRef) -> Result<Flow, String> {
        if let Stmt::Block(stmts) = stmt {
            for stmt in stmts { match self.exec(stmt, env.clone())? { Flow::Normal => (), flow => return Ok(flow) } }
            Ok(Flow::Normal)
        } else { self.exec(stmt, env) }
    }
    fn exec(&mut self, stmt: &Stmt, env: EnvRef) -> Result<Flow, String> {
        match stmt {
            Stmt::Empty => Ok(Flow::Normal),
            Stmt::Expr(expr) => { self.eval(expr, env)?; Ok(Flow::Normal) }
            Stmt::Block(stmts) => {
                let local = Env::new(Some(env));
                for stmt in stmts { match self.exec(stmt, local.clone())? { Flow::Normal => (), flow => return Ok(flow) } }
                Ok(Flow::Normal)
            }
            Stmt::Declare(name, size, init, _constant) => {
                let value = if let Some(expr) = init { self.eval(expr, env.clone())? } else if let Some(size) = size { let n = self.eval(size, env.clone())?.integer()?.clamp(0, 1_000_000) as usize; Value::Array(Rc::new(RefCell::new(vec![Value::Int(0); n]))) } else if name.starts_with('@') { Value::Array(Rc::new(RefCell::new(Vec::new()))) } else if name.starts_with('#') { Value::Hash(Rc::new(RefCell::new(HashMap::new()))) } else { Value::Int(0) };
                env.define(name.clone(), value); Ok(Flow::Normal)
            }
            Stmt::Function(name, params, body) => {
                let cell = env.define(name.clone(), Value::Null);
                *cell.borrow_mut() = Value::Function(Rc::new(Function { params: params.clone(), body: (**body).clone(), closure: env }));
                Ok(Flow::Normal)
            }
            Stmt::If(cond, yes, no) => if self.eval(cond, env.clone())?.truthy() { self.exec(yes, env) } else if let Some(no) = no { self.exec(no, env) } else { Ok(Flow::Normal) },
            Stmt::While(cond, body) => {
                while self.eval(cond, env.clone())?.truthy() { match self.exec(body, env.clone())? { Flow::Normal | Flow::Continue => (), Flow::Break => break, flow @ Flow::Return(_) => return Ok(flow) } }
                Ok(Flow::Normal)
            }
            Stmt::DoWhile(body, cond) => {
                loop { match self.exec(body, env.clone())? { Flow::Normal | Flow::Continue => (), Flow::Break => break, flow @ Flow::Return(_) => return Ok(flow) } if !self.eval(cond, env.clone())?.truthy() { break; } }
                Ok(Flow::Normal)
            }
            Stmt::For(init, cond, iter, body) => {
                let local = Env::new(Some(env));
                if let Some(init) = init { self.exec(init, local.clone())?; }
                loop {
                    if let Some(cond) = cond { if !self.eval(cond, local.clone())?.truthy() { break; } }
                    match self.exec(body, local.clone())? { Flow::Normal | Flow::Continue => (), Flow::Break => break, flow @ Flow::Return(_) => return Ok(flow) }
                    if let Some(iter) = iter { self.eval(iter, local.clone())?; }
                }
                Ok(Flow::Normal)
            }
            Stmt::ForEach(name, source, body) => {
                let values = match self.eval(source, env.clone())? { Value::Array(v) => v.borrow().clone(), _ => return Err("foreach expects an array".into()) };
                for value in values {
                    let local = Env::new(Some(env.clone())); local.define(name.clone(), value);
                    match self.exec(body, local)? { Flow::Normal | Flow::Continue => (), Flow::Break => break, flow @ Flow::Return(_) => return Ok(flow) }
                }
                Ok(Flow::Normal)
            }
            Stmt::Switch(value, cases, default) => {
                let target = self.eval(value, env.clone())?;
                let mut matched = false;
                for (label, body) in cases {
                    if matched || equal(&target, &self.eval(label, env.clone())?) {
                        matched = true;
                        for stmt in body {
                            match self.exec(stmt, env.clone())? {
                                Flow::Normal | Flow::Continue => (),
                                Flow::Break => return Ok(Flow::Normal),
                                flow @ Flow::Return(_) => return Ok(flow),
                            }
                        }
                    }
                }
                if let Some(body) = default {
                    for stmt in body {
                        match self.exec(stmt, env.clone())? {
                            Flow::Normal | Flow::Continue => (),
                            Flow::Break => break,
                            flow @ Flow::Return(_) => return Ok(flow),
                        }
                    }
                }
                Ok(Flow::Normal)
            }
            Stmt::Return(expr) => Ok(Flow::Return(if let Some(expr) = expr { self.eval(expr, env)? } else { Value::Int(0) })),
            Stmt::Break => Ok(Flow::Break), Stmt::Continue => Ok(Flow::Continue),
            Stmt::Import(namespace) => { self.imports.push(namespace.clone()); Ok(Flow::Normal) }
        }
    }

    fn eval(&mut self, expr: &Expr, env: EnvRef) -> Result<Value, String> {
        match expr {
            Expr::Literal(Value::Str(value)) => Ok(Value::Str(self.interpolate(value, env)?)),
            Expr::Literal(value) => Ok(value.clone()),
            Expr::Variable(name) => {
                if let Some(cell) = env.get_cell(name) { return Ok(cell.borrow().clone()); }
                for namespace in self.imports.iter().rev() {
                    if let Some(cell) = env.get_cell(&format!("{namespace}::{name}")) { return Ok(cell.borrow().clone()); }
                }
                Err(format!("undefined variable '{name}'"))
            }
            Expr::Array(items) => Ok(Value::Array(Rc::new(RefCell::new(items.iter().map(|item| self.eval(item, env.clone())).collect::<Result<Vec<_>,_>>()?)))),
            Expr::Hash(items) => { let mut map = HashMap::new(); for (k,v) in items { map.insert(self.eval(k, env.clone())?.string(), self.eval(v, env.clone())?); } Ok(Value::Hash(Rc::new(RefCell::new(map)))) }
            Expr::Unary(op, value) => {
                if op == "++" || op == "--" { let delta = if op == "++" { 1 } else { -1 }; return self.update_lvalue(value, env, delta, true); }
                let value = self.eval(value, env)?;
                match op.as_str() { "-" => Ok(Value::Real(-value.number()?)), "+" => Ok(Value::Real(value.number()?)), "!" | "not" => Ok(Value::Bool(!value.truthy())), "~" => Ok(Value::Int(!value.integer()?)), _ => Err(format!("unknown unary operator {op}")) }
            }
            Expr::Binary(lhs, op, rhs) => {
                let left = self.eval(lhs, env.clone())?;
                if op == "and" || op == "&&" { return if left.truthy() { Ok(Value::Bool(self.eval(rhs, env)?.truthy())) } else { Ok(Value::Bool(false)) }; }
                if op == "or" || op == "||" { return if left.truthy() { Ok(Value::Bool(true)) } else { Ok(Value::Bool(self.eval(rhs, env)?.truthy())) }; }
                let right = self.eval(rhs, env)?; self.binary(left, op, right)
            }
            Expr::Assign(lhs, op, rhs) => {
                let value = self.eval(rhs, env.clone())?;
                let value = if op == "=" { value } else { let old = self.eval(lhs, env.clone())?; self.binary(old, &op[..1], value)? };
                self.set_lvalue(lhs, value.clone(), env)?; Ok(value)
            }
            Expr::Index(base, index) => {
                let base = self.eval(base, env.clone())?; let index = self.eval(index, env)?;
                match base { Value::Array(values) => Ok(values.borrow().get(index.integer()? as usize).cloned().unwrap_or(Value::Int(0))), Value::Hash(values) => Ok(values.borrow().get(&index.string()).cloned().unwrap_or(Value::Int(0))), Value::Str(v) => Ok(Value::Str(v.chars().nth(index.integer()? as usize).unwrap_or('\0').to_string())), _ => Err("indexing requires an array, hash, or string".into()) }
            }
            Expr::Call(callee, args) => { let function = self.eval(callee, env.clone())?; let args = args.iter().map(|a| self.eval(a, env.clone())).collect::<Result<Vec<_>,_>>()?; self.call(function, args, env) }
            Expr::Function(params, body) => Ok(Value::Function(Rc::new(Function { params: params.clone(), body: (**body).clone(), closure: env }))),
            Expr::Postfix(target, op) => {
                let old = self.eval(target, env.clone())?; let delta = if op == "++" { 1 } else { -1 };
                let next = self.binary(old.clone(), "+", Value::Int(delta))?; self.set_lvalue(target, next, env)?; Ok(old)
            }
            Expr::Sequence(lhs, rhs) => { self.eval(lhs, env.clone())?; self.eval(rhs, env) }
        }
    }

    fn interpolate(&mut self, source: &str, env: EnvRef) -> Result<String, String> {
        let chars: Vec<char> = source.chars().collect();
        let mut output = String::new();
        let mut i = 0;
        while i < chars.len() {
            match chars[i] {
                '\u{e000}' => { output.push('{'); i += 1; }
                '\u{e001}' => { output.push('}'); i += 1; }
                '{' => {
                    if let Some(offset) = chars[i + 1..].iter().position(|ch| *ch == '}') {
                        let end = i + 1 + offset;
                        let expression: String = chars[i + 1..end].iter().collect();
                        let evaluated = (|| {
                            let tokens = Lexer::new(&expression).tokens()?;
                            let mut parser = Parser::new(tokens);
                            let expr = parser.expression(0)?;
                            if !matches!(parser.token().kind, TokenKind::Eof) { return Err("trailing tokens in interpolation".to_owned()); }
                            self.eval(&expr, env.clone()).map(|value| value.string())
                        })();
                        output.push_str(&evaluated.unwrap_or_else(|_| "%err%".to_owned()));
                        i = end + 1;
                    } else { output.push('{'); i += 1; }
                }
                ch => { output.push(ch); i += 1; }
            }
        }
        Ok(output)
    }

    fn binary(&self, a: Value, op: &str, b: Value) -> Result<Value, String> {
        match op {
            "+" => match (&a,&b) { (Value::Str(_),_) | (_,Value::Str(_)) => Ok(Value::Str(a.string()+&b.string())), (Value::Array(x),Value::Array(y)) => { let mut v=x.borrow().clone(); v.extend(y.borrow().clone()); Ok(Value::Array(Rc::new(RefCell::new(v)))) }, (Value::Hash(x),Value::Hash(y)) => { let mut m=x.borrow().clone(); m.extend(y.borrow().clone()); Ok(Value::Hash(Rc::new(RefCell::new(m)))) }, _ => Ok(Value::Real(a.number()?+b.number()?)) },
            "-" => match (&a, &b) { (Value::Hash(values), Value::Str(key)) => { let mut values = values.borrow().clone(); values.remove(key); Ok(Value::Hash(Rc::new(RefCell::new(values)))) }, _ => Ok(Value::Real(a.number()? - b.number()?)) },
            "*" => Ok(Value::Real(a.number()?*b.number()?)), "/" => Ok(Value::Real(a.number()? / b.number()?)), "%" => Ok(Value::Real(a.number()? % b.number()?)), "^" => Ok(Value::Real(a.number()?.powf(b.number()?))),
            "d" => { let count=a.integer()?.max(0); let sides=b.integer()?.max(1); Ok(Value::Int(if count==0 {0} else { (count*sides+count)/2 })) },
            "==" => Ok(Value::Bool(equal(&a,&b))), "!=" => Ok(Value::Bool(!equal(&a,&b))),
            "<" => Ok(Value::Bool(compare(&a,&b)? < 0)), "<=" => Ok(Value::Bool(compare(&a,&b)? <= 0)), ">" => Ok(Value::Bool(compare(&a,&b)? > 0)), ">=" => Ok(Value::Bool(compare(&a,&b)? >= 0)),
            "|" => Ok(Value::Int(a.integer()? | b.integer()?)), "&" => Ok(Value::Int(a.integer()? & b.integer()?)),
            _ => Err(format!("unknown binary operator {op}")),
        }
    }

    fn set_lvalue(&mut self, target: &Expr, value: Value, env: EnvRef) -> Result<(), String> {
        match target {
            Expr::Variable(name) => {
                if let Some(cell) = env.get_cell(name) { *cell.borrow_mut() = value; }
                else { env.define(name.clone(), value); }
                Ok(())
            }
            Expr::Index(base,index) => {
                let base_value = self.eval(base, env.clone())?; let index = self.eval(index, env)?;
                match base_value {
                    Value::Array(items) => { let i=index.integer()?; if i<0 {return Err("negative array index".into())}; if i >= 1_000_000 {return Err("array index exceeds the maximum supported size (999999)".into())}; let mut items=items.borrow_mut(); if i as usize>=items.len() {items.resize(i as usize+1,Value::Int(0));} items[i as usize]=value; Ok(()) },
                    Value::Hash(items) => { items.borrow_mut().insert(index.string(),value); Ok(()) },
                    _ => Err("assignment target is not mutable".into()),
                }
            }
            _ => Err("invalid assignment target".into()),
        }
    }

    fn update_lvalue(&mut self, target: &Expr, env: EnvRef, delta: i64, prefix: bool) -> Result<Value, String> {
        let old = self.eval(target, env.clone())?; let next = self.binary(old.clone(), "+", Value::Int(delta))?;
        self.set_lvalue(target, next.clone(), env)?; Ok(if prefix { next } else { old })
    }

    fn call(&mut self, callable: Value, args: Vec<Value>, caller: EnvRef) -> Result<Value, String> {
        match callable {
            Value::Function(function) => {
                let env=Env::new(Some(function.closure.clone()));
                for (i,param) in function.params.iter().enumerate() {
                    let value=if let Some(v)=args.get(i) {v.clone()} else if let Some(expr)=&param.default {self.eval(expr,env.clone())?} else {Value::Int(0)};
                    env.define(param.name.clone(),value);
                }
                match self.exec(&function.body,env)? {Flow::Return(v)=>Ok(v),_=>Ok(Value::Int(0))}
            }
            Value::Native(name) => self.native(&name,args,caller),
            _ => Err("value is not callable".into()),
        }
    }

    fn native(&mut self, name: &str, args: Vec<Value>, env: EnvRef) -> Result<Value, String> {
        let name = name.strip_prefix("math::").unwrap_or(name);
        let arg = |i: usize| args.get(i).cloned().unwrap_or(Value::Int(0));
        let num = |i: usize| arg(i).number();
        let int = |i: usize| arg(i).integer();
        let real = |v: f64| Value::Real(v);
        match name {
            "print" | "println" => { let mut out=io::stdout().lock(); for v in &args {write!(out,"{}",v.string()).map_err(|e|e.to_string())?;} if name=="println" {writeln!(out).map_err(|e|e.to_string())?;} Ok(Value::Int(0)) },
            "len" => { let value = arg(0); Ok(Value::Int(match &value {Value::Str(v)=>v.chars().count() as i64,Value::Array(v)=>v.borrow().len() as i64,Value::Hash(v)=>v.borrow().len() as i64,_=>return Err(format!("len expects a string, array, or hash; got {}", value.kind()))})) },
            "real" => Ok(real(num(0)?)), "integer" => Ok(Value::Int(int(0)?)), "boolean" => Ok(Value::Bool(arg(0).truthy())), "string" => Ok(Value::Str(arg(0).string())), "strlen" => Ok(Value::Int(arg(0).string().len() as i64)),
            "substr" => {let s=arg(0).string(); let chars:Vec<char>=s.chars().collect(); let start=int(1)?.max(0) as usize; let length=int(2)?.max(0) as usize; Ok(Value::Str(chars.into_iter().skip(start).take(length).collect()))},
            "is_array" => Ok(Value::Bool(matches!(arg(0),Value::Array(_)))), "is_function" => Ok(Value::Bool(matches!(arg(0),Value::Function(_) | Value::Native(_)))), "is_handle" | "is_valid" => Ok(Value::Bool(false)),
            "array" => {let mut all=Vec::new(); for v in args {if let Value::Array(a)=v {all.extend(a.borrow().clone())} else {all.push(v)}} Ok(Value::Array(Rc::new(RefCell::new(all))))}, "hash" => Ok(Value::Hash(Rc::new(RefCell::new(HashMap::new())))),
            "__pop" | "__popb" => {let Value::Array(a)=arg(0) else{return Err("pop expects an array".into())}; let mut a=a.borrow_mut(); Ok(if name=="__pop" {if a.is_empty(){Value::Int(0)}else{a.remove(0)}} else {a.pop().unwrap_or(Value::Int(0))})},
            "__push" | "__pushb" => {let Value::Array(a)=arg(0) else{return Err("push expects an array".into())}; if name=="__push" {a.borrow_mut().insert(0,arg(1))} else {a.borrow_mut().push(arg(1))}; Ok(Value::Array(a))},
            "__remove" => {let a=arg(0); let key=arg(1); match a {Value::Array(a)=>{let i=key.integer()? as usize;let mut a=a.borrow_mut();Ok(if i<a.len(){a.remove(i)}else{Value::Int(0)})},Value::Hash(a)=>Ok(a.borrow_mut().remove(&key.string()).unwrap_or(Value::Int(0))),_=>Err("remove expects an array or hash".into())}},
            "floor"=>Ok(real(num(0)?.floor())),"ceil"=>Ok(real(num(0)?.ceil())),"round"=>Ok(real(num(0)?.round())),"abs"=>Ok(real(num(0)?.abs())),"exp"=>Ok(real(num(0)?.exp())),"log"=>Ok(real(num(0)?.ln())),"log10"=>Ok(real(num(0)?.log10())),"sqrt"=>Ok(real(num(0)?.sqrt())),"sin"=>Ok(real(num(0)?.sin())),"cos"=>Ok(real(num(0)?.cos())),"tan"=>Ok(real(num(0)?.tan())),"asin"=>Ok(real(num(0)?.asin())),"acos"=>Ok(real(num(0)?.acos())),"atan"=>Ok(real(num(0)?.atan())),"atan2"=>Ok(real(num(0)?.atan2(num(1)?))),"sinh"=>Ok(real(num(0)?.sinh())),"cosh"=>Ok(real(num(0)?.cosh())),"tanh"=>Ok(real(num(0)?.tanh())),"pow"=>Ok(real(num(0)?.powf(num(1)?))),
            "rad2deg"=>Ok(real(num(0)?.to_degrees())),"deg2rad"=>Ok(real(num(0)?.to_radians())),"rand"=>Ok(Value::Int(4)),"randf"=>Ok(real(0.5)),"srand"=>Ok(Value::Int(0)),
            "require" => {let file=arg(0).string(); let path=std::path::Path::new(&self.script_dir).join(file); let source=fs::read_to_string(&path).map_err(|e|format!("could not require {}: {e}",path.display()))?; let tokens=Lexer::new(&source).tokens()?; let mut parser=Parser::new(tokens); let stmt=parser.program()?; self.exec_program(&stmt,env)?; Ok(Value::Int(0))},
            _ => Err(format!("unknown function '{name}'")),
        }
    }
}

fn equal(a: &Value,b: &Value)->bool {
    match(a,b){(Value::Null,Value::Null)=>true,(Value::Bool(a),Value::Bool(b))=>a==b,(Value::Int(a),Value::Int(b))=>a==b,(Value::Real(a),Value::Real(b))=>a==b,(Value::Int(a),Value::Real(b))=>*a as f64==*b,(Value::Real(a),Value::Int(b))=>*a==*b as f64,(Value::Str(a),Value::Str(b))=>a==b,_=>false}
}
fn compare(a:&Value,b:&Value)->Result<i8,String>{ if let (Value::Str(a),Value::Str(b))=(a,b){Ok(if a<b{-1}else if a>b{1}else{0})}else{let a=a.number()?;let b=b.number()?;Ok(if a<b{-1}else if a>b{1}else{0})} }
fn main() {
    let mut filename=None; let mut no_exec=false; let mut show_version=false;
    let mut args=env::args().skip(1);
    while let Some(arg)=args.next(){match arg.as_str(){"-v"|"--version"=>show_version=true,"-n"|"--no-exec"=>no_exec=true,"-h"|"--help"=>{println!("steel-rs 0.1.0\nsteel-rs [--no-exec] <file|->");return;},"-"=>filename=Some(arg),_ if arg.starts_with('-')=>{eprintln!("unknown option: {arg}");std::process::exit(2)},_=>if filename.replace(arg).is_some(){eprintln!("only one script file may be specified");std::process::exit(2)}}}
    if show_version {println!("steel-rs 0.1.0");return;}
    let Some(filename)=filename else{eprintln!("usage: steel-rs [--no-exec] [--print] <file|->");std::process::exit(2)};
    let source=if filename=="-"{let mut s=String::new();if let Err(e)=io::stdin().read_to_string(&mut s){eprintln!("stdin: {e}");std::process::exit(1)}s}else{match fs::read_to_string(&filename){Ok(v)=>v,Err(e)=>{eprintln!("couldn't open file {filename}: {e}");std::process::exit(1)}}};
    let result=(||{let tokens=Lexer::new(&source).tokens()?;let mut parser=Parser::new(tokens);let program=parser.program()?;if !no_exec {let dir=std::path::Path::new(&filename).parent().unwrap_or(std::path::Path::new("."));let mut runtime=Runtime::new(dir.to_string_lossy().into_owned());runtime.run(&program)?;}Ok::<(),String>(())})();
    if let Err(error)=result {eprintln!("{filename}: {error}");std::process::exit(1);}
}
