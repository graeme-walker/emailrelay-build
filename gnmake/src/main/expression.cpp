//
// Copyright (c) 2023 Graeme Walker <graeme_walker@users.sf.net>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// ===
///
/// \file expression.cpp
///

#include "gdef.h"
#include "gstr.h"
#include "gstringview.h"
#include "expression.h"
#include <cctype>
#include <limits>
#include <memory>
#include <stdexcept>
#include <array>
#include <deque>

Expression::Error::Error() :
	std::runtime_error("invalid expression")
{
}

Expression::Error::Error( const std::string & more ) :
	std::runtime_error("invalid expression: "+more)
{
}

Expression::Error::Error( const std::string & more1 , const std::string & more2 ) :
	std::runtime_error("invalid expression: "+more1+": "+more2)
{
}

struct Token /// lexical token, with variables already expanded
{
	enum class Type { open , close , number , string , infix } ;
	Type type ;
	std::string str ;
} ;
std::ostream & operator<<( std::ostream & s , const Token & t ) ;
using Tokens = std::deque<Token> ;

namespace Lexer
{
	void tokenise( const std::string & , Tokens & , const Vars & vars , const Expander & ) ;
	static bool isdigit( std::string::const_iterator , std::string::const_iterator , std::string::const_iterator ) ;
	static bool isoperator( std::string::const_iterator , std::string::const_iterator ) ;
}

struct Value /// constant value: number or string
{
	Value() ;
	Value( int ) ; // implicit
	Value( const std::string & ) ; // implicit
	explicit Value( const Token & ) ;
	bool is_number ;
	int number ;
	std::string str ;
	bool operator()() const ;
	int n() const ;
	std::string s() const ;
	using Function1 = Value (*)( Value ) ;
	using Function2 = Value (*)( Value , Value ) ;
} ;

struct Node ;
using NodePtr = std::shared_ptr<Node> ;

struct Node /// AST node: value or function, with links to function parameter nodes
{
	Node() ;
	explicit Node( const Value & v , const std::string & debug = {} ) ;
	explicit Node( Value::Function1 , NodePtr , const std::string & debug = {} ) ;
	explicit Node( Value::Function2 , NodePtr , NodePtr , const std::string & debug = {} ) ;
	Value value ; // if no function
	Value::Function1 function1 {nullptr} ;
	Value::Function2 function2 {nullptr} ;
	NodePtr p1 {nullptr} ;
	NodePtr p2 {nullptr} ;
	std::string debug ;
	static void show( std::ostream & , NodePtr tree , int = 0 ) ;
	Value evaluate() const ;
	static void assert_( bool ) ;
} ;

template <typename... Args>
NodePtr new_Node( Args&&... args )
{
	return std::make_unique<Node>( std::forward<Args>(args)... ) ;
}

// ==

Value::Value() :
	is_number(true) ,
	number(0)
{
}

Value::Value( int n ) :
	is_number(true) ,
	number(n) ,
	str(G::Str::fromInt(n))
{
}

Value::Value( const std::string & s ) :
	is_number(false) ,
	number(G::Str::toInt(s,"0")) ,
	str(s)
{
}

Value::Value( const Token & token ) :
	is_number(token.type==Token::Type::number) ,
	number(0) ,
	str(token.str)
{
	if( is_number )
	{
		char * end = nullptr ;
		long n = std::strtol( str.c_str() , &end , 0 ) ; // decimal, octal, hex
		if( end != ( str.c_str() + str.size() ) ||
			n > std::numeric_limits<int>::max() ||
			n < std::numeric_limits<int>::min() )
				throw Expression::Error( "invalid number" , str ) ;
		number = static_cast<int>(n) ;
	}
}

bool Value::operator()() const
{
	return is_number ? ( number != 0 ) : !str.empty() ;
}

int Value::n() const
{
	if( !is_number )
		throw Expression::Error( "not a number" , str ) ;
	return number ;
}

std::string Value::s() const
{
	if( is_number )
		throw Expression::Error( "not a string" ) ;
	return str ;
}

// ==

Node::Node() :
	debug("empty")
{
}

Node::Node( const Value & v , const std::string & d ) :
	value(v) ,
	debug(d)
{
}

Node::Node( Value::Function1 fn1 , NodePtr n , const std::string & d ) :
	function1(fn1) ,
	p1(n) ,
	debug(d)
{
}

Node::Node( Value::Function2 fn2 , NodePtr lhs , NodePtr rhs , const std::string & d ) :
	function2(fn2) ,
	p1(lhs) ,
	p2(rhs) ,
	debug(d)
{
	assert_( lhs != 0 ) ;
	assert_( rhs != 0 ) ;
}

Value Node::evaluate() const
{
	assert_( !function1 || p1 ) ;
	assert_( !function2 || (p1 && p2) ) ;
	if( function2 )
		return function2( p1->evaluate() , p2->evaluate() ) ;
	else if( function1 )
		return function1( p1->evaluate() ) ;
	else
		return value ;
}

void Node::show( std::ostream & s , NodePtr p , int depth )
{
	if( p )
	{
		for( int i = -1 ; i < depth ; i++ ) s << "  " ;
		s << "[" << p->debug << "][" << p->value.str << "]\n" ;
		if( p->p1 ) show( s , p->p1 , depth+1 ) ;
		if( p->p2 ) show( s , p->p2 , depth+1 ) ;
	}
}

void Node::assert_( bool b )
{
	if( !b )
		throw Expression::Error() ;
}

// ==

namespace operators
{
	Value not_( Value a )
	{
		return a() ? 0 : 1 ;
	}
	Value minus( Value a )
	{
		return -a.n() ;
	}
	Value mult( Value a , Value b )
	{
		return a.n() * b.n() ;
	}
	Value divide( Value a , Value b )
	{
		if( b() == 0 )
			throw Expression::Error( "divide by zero" ) ;
		return a.n() / b.n() ;
	}
	Value add( Value a , Value b )
	{
		return a.n() + b.n() ;
	}
	Value subtract( Value a , Value b )
	{
		return a.n() - b.n() ;
	}
	Value lt( Value a , Value b )
	{
		return a.n() < b.n() ;
	}
	Value gt( Value a , Value b )
	{
		return a.n() > b.n() ;
	}
	Value gte( Value a , Value b )
	{
		return a.n() >= b.n() ;
	}
	Value lte( Value a , Value b )
	{
		return a.n() <= b.n() ;
	}
	Value eq( Value a , Value b )
	{
		return a.is_number ? ( a.n() == b.n() ) : ( a.s() == b.s() ) ;
	}
	Value ne( Value a , Value b )
	{
		return a.is_number ? ( a.n() != b.n() ) : ( a.s() != b.s() ) ;
	}
	Value bitwise_and( Value a , Value b )
	{
		return a.n() & b.n() ;
	}
	Value bitwise_or( Value a , Value b )
	{
		return a.n() | b.n() ;
	}
	Value and_( Value a , Value b )
	{
		return a() && b() ;
	}
	Value or_( Value a , Value b )
	{
		return a() || b() ;
	}
	struct Op
	{
		G::string_view s ;
		int arity ;
		Value (*fn2)(Value,Value) ;
		Value (*fn1)(Value) ;
	} ;
	static std::array<Op,16> ops = {{
		{ "*" , 2 , mult , 0 } ,
		{ "/" , 2 , divide , 0 } ,
		{ "+" , 2 , add , 0 } ,
		{ "-" , 2 , subtract , 0 } ,
		{ "<" , 2 , lt , 0 } ,
		{ ">" , 2 , gt , 0 } ,
		{ ">=" , 2 , gte , 0 } ,
		{ "<=" , 2 , lte , 0 } ,
		{ "==" , 2 , eq , 0 } ,
		{ "!=" , 2 , ne , 0 } ,
		{ "&" , 2 , bitwise_and , 0 } ,
		{ "|" , 2 , bitwise_or , 0 } ,
		{ "&&" , 2 , and_ , 0 } ,
		{ "||" , 2 , or_ , 0 } ,
		{ "!" , 1 , 0 , not_ } ,
		{ "-" , 1 , 0 , minus }
	}} ;
}

// ==

namespace Grammar
{
	using Strings = std::vector<G::string_view> ;
	using Function = NodePtr (*)( Tokens & ) ;

	bool op_match1( const Strings & ss , const std::string & token , Value::Function1 & fn_out )
	{
		for( const auto & op : operators::ops )
		{
			if( op.arity == 1 && std::find(ss.begin(),ss.end(),op.s)!=ss.end() && op.s == token )
			{
				fn_out = op.fn1 ;
				return true ;
			}
		}
		return false ;
	}

	bool op_match2( const Strings & ss , const std::string & token , Value::Function2 & fn_out )
	{
		for( const auto & op : operators::ops )
		{
			if( op.arity == 2 && std::find(ss.begin(),ss.end(),op.s)!=ss.end() && op.s == token )
			{
				fn_out = op.fn2 ;
				return true ;
			}
		}
		return false ;
	}

	NodePtr infix1( const Strings & ops , Tokens & tokens , Function grammar_next )
	{
		if( tokens.empty() )
			throw Expression::Error( "missing token" ) ;
		Value::Function1 fn {} ;
		NodePtr node = 0 ;
		NodePtr first = 0 ;
		while( !tokens.empty() && tokens.front().type == Token::Type::infix && op_match1( ops , tokens.front().str , fn ) )
		{
			std::string debug = tokens.front().str ;
			tokens.pop_front() ;
			node = new_Node( fn , node , debug ) ;
			if( !first ) first = node ;
		}
		if( node )
		{
			Node::assert_( first->p1 == 0 ) ;
			first->p1 = grammar_next( tokens ) ;
			return node ;
		}
		else
		{
			return grammar_next( tokens ) ;
		}
	}

	NodePtr infix2( const Strings & ops , Tokens & tokens , Function grammar_next )
	{
		if( tokens.empty() )
			throw Expression::Error( "missing token" ) ;
		auto node = grammar_next( tokens ) ;
		Value::Function2 fn {} ;
		while( !tokens.empty() && tokens.front().type == Token::Type::infix && op_match2( ops , tokens.front().str , fn ) )
		{
			std::string debug = tokens.front().str ;
			tokens.pop_front() ;
			auto other_node = grammar_next( tokens ) ;
			node = new_Node( fn , node , other_node , debug ) ;
		}
		return node ;
	}

	NodePtr top( Tokens & ) ;

	NodePtr base( Tokens & tokens )
	{
		NodePtr node {} ;
		if( tokens.empty() )
		{
			throw Expression::Error( "missing token" ) ;
		}
		else if( tokens.front().type == Token::Type::number )
		{
			node = new_Node( Value(tokens.front()) ) ;
			node->debug = "#" ;
			tokens.pop_front() ;
		}
		else if( tokens.front().type == Token::Type::string )
		{
			node = new_Node( Value(tokens.front()) ) ;
			node->debug = "'" ;
			tokens.pop_front() ;
		}
		else if( tokens.front().type == Token::Type::infix )
		{
		}
		else if( tokens.front().type == Token::Type::open )
		{
			tokens.pop_front() ;
			node = top( tokens ) ;
			if( !tokens.empty() && tokens.front().type == Token::Type::close )
				tokens.pop_front() ;
			else
				throw Expression::Error( "no closing round bracket" ) ;
		}
		return node ;
	}

	// grammar -- operators in precedence order
	NodePtr minusnot( Tokens & tokens ) { return infix1( {"-","!"} , tokens , base ) ; }
	NodePtr muldiv( Tokens & tokens ) { return infix2( {"*","/"} , tokens , minusnot ) ; }
	NodePtr addsub( Tokens & tokens ) { return infix2( {"+","-"} , tokens , muldiv ) ; }
	NodePtr cmp( Tokens & tokens ) { return infix2( {"<",">",">=","<="} , tokens , addsub ) ; }
	NodePtr eqne( Tokens & tokens ) { return infix2( {"==","!="} , tokens , cmp ) ; }
	NodePtr bw_and( Tokens & tokens ) { return infix2( {"&"} , tokens , eqne ) ; }
	NodePtr bw_or( Tokens & tokens ) { return infix2( {"|"} , tokens , bw_and ) ; }
	NodePtr and_( Tokens & tokens ) { return infix2( {"&&"} , tokens , bw_or ) ; }
	NodePtr or_( Tokens & tokens ) { return infix2( {"||"} , tokens , and_ ) ; }

	NodePtr top( Tokens & tokens )
	{
		if( tokens.empty() ) return new_Node() ;
		return or_( tokens ) ;
	}
}

// ==

int Expression::evaluate( const std::string & expr , const Expander & expander , const Vars & vars , bool debug )
{
	Tokens tokens ;
	Lexer::tokenise( expr , tokens , vars , expander ) ;
	if( debug ) for( auto p : tokens ) std::cout << "  token: [" << p << "]\n" ;
	NodePtr tree = Grammar::top( tokens ) ;
	if( debug ) Node::show( std::cout , tree , 0 ) ;
	Value result = tree->evaluate() ;
	return result.is_number ? result.n() : (result()?1:0) ;
}

// ==

void Lexer::tokenise( const std::string & expr , Tokens & tokens ,
	const Vars & vars , const Expander & expander )
{
	for( auto p = expr.begin() ; p != expr.end() ; )
	{
		if( std::isdigit(*p) )
		{
			auto q = p ;
			for( ; q != expr.end() && isdigit(q,p,expr.end()) ; q++ )
				{;}
			tokens.push_back( Token{Token::Type::number,std::string(p,q)} ) ;
			if( (p=q) == expr.end() )
				break ;
		}
		else if( *p == '"' )
		{
			auto q = p ;
			for( ++q ; q != expr.end() && *q != '"' ; q++ )
				{;}
			std::string str = std::string( std::next(p) , q ) ;
			str = expander.expand( str , vars ) ;
			tokens.push_back( Token{Token::Type::string,str} ) ;
			if( (p=++q) == expr.end() )
				break ;
		}
		else if( *p == '(' )
		{
			tokens.push_back( Token{Token::Type::open,{}} ) ;
			++p ;
		}
		else if( *p == ')' )
		{
			tokens.push_back( Token{Token::Type::close,{}} ) ;
			++p ;
		}
		else if( !std::isspace(*p) && !std::isdigit(*p) )
		{
			auto q = p ;
			for( ++q ; q != expr.end() && !std::isspace(*q) && !std::isdigit(*q) &&
				!(isoperator(p,q) && !isoperator(p,std::next(q))) ; ++q )
					{;}
			Token::Type tt ;
			std::string str = std::string( p , q ) ;
			tt = Token::Type::infix ;
			tokens.push_back( Token{tt,str} ) ;
			if( (p=q) == expr.end() )
				break ;
		}
		else
		{
			++p ;
		}
	}
}

bool Lexer::isoperator( std::string::const_iterator start , std::string::const_iterator end )
{
	using Op = operators::Op ;
	std::string s( start , end ) ;
	bool rc = operators::ops.end() != std::find_if( operators::ops.begin() , operators::ops.end() ,
		[=](const Op & op_){ return op_.s == s ; } ) ;
	return rc ;
}

bool Lexer::isdigit( std::string::const_iterator p , std::string::const_iterator start , std::string::const_iterator end )
{
	bool hex = false ;
	if( std::distance(start,p) >= 1U && std::next(start) != end )
	{
		hex = *std::next(start) == 'x' || *std::next(start) == 'X' ;
		if( hex && std::distance(start,p) == 1U )
			return true ;
	}
	return
		std::isdigit(*p) || ( hex && ( ( *p >= 'a' && *p <= 'f' ) || ( *p >= 'A' && *p <= 'F' ) ) ) ;
}

// ==

std::ostream & operator<<( std::ostream & s , const Token::Type & tt )
{
	if( tt == Token::Type::open ) return s << "open" ;
	if( tt == Token::Type::close ) return s << "close" ;
	if( tt == Token::Type::number ) return s << "number" ;
	if( tt == Token::Type::string ) return s << "string" ;
	if( tt == Token::Type::infix ) return s << "infix" ;
	return s ;
}

std::ostream & operator<<( std::ostream & s , const Token & t )
{
	return s << t.type << ":" << t.str ;
}

