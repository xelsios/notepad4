#include "EditLexer.h"
#include "EditStyleX.h"

static KEYWORDLIST Keywords_AS = {{
//++Autogenerated -- start of section automatically generated
"Infinity NaN as break case catch class const continue default defer delete do dynamic each else extends "
"false final finally for function get if implements in include instanceof interface internal intrinsic is let "
"namespace native new null override package private protected public rest return set static super switch "
"this throw true try typeof undefined use var while with "

, // 1 types
"int uint void "

, // 2 directive
"import "

, // 3 class
"ArgumentError Array Boolean Class Date DefinitionError Error EvalError Function JSON Math Namespace Null Number Object "
"QName RangeError ReferenceError RegExp SecurityError String SyntaxError TypeError URIError Vector VerifyError "
"XML XMLList arguments "

, // 4 interface
NULL

, // 5 enumeration
NULL

, // 6 constant
NULL

, // 7 metadata
NULL

, // 8 function
"decodeURI( decodeURIComponent( encodeURI( encodeURIComponent( escape( isFinite( isNaN( isXMLName( parseFloat( parseInt( "
"trace( unescape( "

, // 9 properties
NULL

, // 10 doc tag
NULL

, NULL, NULL, NULL, NULL
//--Autogenerated -- end of section automatically generated

, // 15 Code Snippet
"for^() if^() switch^() while^() else^if^() else^{} "
}};

static EDITSTYLE Styles_AS[] = {
	EDITSTYLE_DEFAULT,
	{ MULTI_STYLE(SCE_JS_WORD, SCE_JS_DIRECTIVE, 0, 0), NP2StyleX_Keyword, L"fore:#0000FF" },
	{ SCE_JS_WORD2, NP2StyleX_TypeKeyword, L"fore:#0000FF" },
	{ SCE_JS_CLASS, NP2StyleX_Class, L"bold; fore:#0080C0" },
	{ SCE_JS_INTERFACE, NP2StyleX_Interface, L"bold; fore:#1E90FF" },
	{ SCE_JS_DECORATOR, NP2StyleX_Metadata, L"fore:#FF8000" },
	{ SCE_JS_FUNCTION_DEFINITION, NP2StyleX_FunctionDefinition, L"bold; fore:#A46000" },
	{ SCE_JS_FUNCTION, NP2StyleX_Function, L"fore:#A46000" },
	{ MULTI_STYLE(SCE_JS_COMMENTLINE, SCE_JS_COMMENTBLOCK, 0, 0), NP2StyleX_Comment, L"fore:#608060" },
	{ MULTI_STYLE(SCE_JS_COMMENTLINEDOC, SCE_JS_COMMENTBLOCKDOC, 0, 0), NP2StyleX_DocComment, L"fore:#408040" },
	{ MULTI_STYLE(SCE_JS_COMMENTTAGAT, SCE_JS_COMMENTTAGXML, 0, 0), NP2StyleX_DocCommentTag, L"fore:#408080" },
	{ SCE_JS_TASKMARKER, NP2StyleX_TaskMarker, L"bold; fore:#408080" },
	{ MULTI_STYLE(SCE_JS_STRING_SQ, SCE_JS_STRING_DQ, SCE_JSX_STRING_SQ, SCE_JSX_STRING_DQ), NP2StyleX_String, L"fore:#008000" },
	{ SCE_JS_ESCAPECHAR, NP2StyleX_EscapeSequence, L"fore:#0080C0" },
	{ SCE_JS_KEY, NP2StyleX_Property, L"fore:#A46000" },
	{ SCE_JS_REGEX, NP2StyleX_Regex, L"fore:#006633; back:#FFF1A8" },
	{ SCE_JSX_TAG, NP2StyleX_XMLTag, L"fore:#648000" },
	{ MULTI_STYLE(SCE_JSX_ATTRIBUTE, SCE_JSX_ATTRIBUTE_AT, 0, 0), NP2StyleX_XMLAttribute, L"fore:#FF4000" },
	{ SCE_JS_LABEL, NP2StyleX_Label, L"back:#FFC040" },
	{ SCE_JS_NUMBER, NP2StyleX_Number, L"fore:#FF0000" },
	{ MULTI_STYLE(SCE_JS_OPERATOR, SCE_JS_OPERATOR2, SCE_JS_OPERATOR_PF, 0), NP2StyleX_Operator, L"fore:#B000B0" },
};

EDITLEXER lexActionScript = {
	SCLEX_JAVASCRIPT, NP2LEX_ACTIONSCRIPT,
//Settings++Autogenerated -- start of section automatically generated
	{
		LexerAttr_Default,
		TAB_WIDTH_4, INDENT_WIDTH_4,
		(1 << 0) | (1 << 1) | (1 << 2), // class, anonymous object, method
		SCE_JS_FUNCTION_DEFINITION
		, KeywordAttr32(0, KeywordAttr_PreSorted) // keywords
		| KeywordAttr32(1, KeywordAttr_PreSorted) // types
		| KeywordAttr32(2, KeywordAttr_PreSorted) // directive
		| KeywordAttr32(3, KeywordAttr_PreSorted) // class
		| KeywordAttr64(8, KeywordAttr_NoLexer) // function
		| KeywordAttr64(9, KeywordAttr_NoLexer) // properties
		| KeywordAttr64(10, KeywordAttr_NoLexer) // doc tag
	},
//Settings--Autogenerated -- end of section automatically generated
	EDITLEXER_HOLE(L"ActionScript", Styles_AS),
	L"as",
	&Keywords_AS,
	Styles_AS
};
