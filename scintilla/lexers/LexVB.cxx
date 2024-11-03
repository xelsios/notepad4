// Scintilla source code edit control
/** @file LexVB.cxx
 ** Lexer for Visual Basic and VBScript.
 **/
// Copyright 1998-2005 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cassert>
#include <cstring>

#include <string>
#include <string_view>
#include <vector>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "StringUtils.h"
#include "LexerModule.h"

using namespace Lexilla;

namespace {

enum class Language {
	VBNET,
	VBA,
	VBScript,
};

enum class KeywordType {
	None,
	End,
	AccessModifier,
	Function,
};

enum {
	VBLineType_CommentLine = 1,
	VBLineType_DimLine = 2,
	VBLineType_ConstLine = 3,
	VBLineType_VB6TypeLine = 4,
	VBLineStateLineContinuation = 1 << 3,
	VBLineStateStringInterpolation = 1 << 4,
};

#define LexCharAt(pos)	styler.SafeGetCharAt(pos)

// https://learn.microsoft.com/en-us/dotnet/visual-basic/reference/language-specification/lexical-grammar#type-characters
// https://learn.microsoft.com/en-us/office/vba/language/reference/user-interface-help/data-type-summary
constexpr bool IsTypeCharacter(int ch) noexcept {
	return ch == '%' // Integer
		|| ch == '&' // Long
		|| ch == '^' // VBA LongLong
		|| ch == '@' // Decimal, VBA Currency
		|| ch == '!' // Single
		|| ch == '#' // Double
		|| ch == '$';// String
}

constexpr bool IsVBNumberPrefix(int ch) noexcept {
	return ch == 'h' || ch == 'H'	// Hexadecimal
		|| ch == 'o' || ch == 'O'	// Octal
		|| ch == 'b' || ch == 'B';	// Binary
}

constexpr bool PreferStringConcat(int chPrevNonWhite, int stylePrevNonWhite) noexcept {
	return chPrevNonWhite == '\"' || chPrevNonWhite == ')' || chPrevNonWhite == ']'
		|| (stylePrevNonWhite != SCE_VB_KEYWORD && IsIdentifierChar(chPrevNonWhite));
}

constexpr bool IsVBNumber(int ch, int chPrev) noexcept {
	return IsHexDigit(ch)|| ch == '_'
		|| (ch == '.' && chPrev != '.')
		|| ((ch == '+' || ch == '-') && (chPrev == 'E' || chPrev == 'e'))
		|| ((ch == 'S' || ch == 'I' || ch == 'L' || ch == 's' || ch == 'i' || ch == 'l')
			&& (IsADigit(chPrev) || chPrev == 'U' || chPrev == 'u'))
		|| ((ch == 'R' || ch == 'r' || ch == '%' || ch == '@' || ch == '!' || ch == '#')
			&& IsADigit(chPrev))
		|| (ch == '&' && IsHexDigit(chPrev));
}

constexpr bool IsSpaceEquiv(int state) noexcept {
	return state <= SCE_VB_LINE_CONTINUATION;
}

// https://docs.microsoft.com/en-us/dotnet/standard/base-types/composite-formatting
constexpr bool IsInvalidFormatSpecifier(int ch) noexcept {
	// Custom format strings allows any characters
	return (ch >= '\0' && ch < ' ') || ch == '\"' || ch == '{' || ch == '}';
}

inline bool IsInterpolatedStringEnd(const StyleContext &sc) noexcept {
	return sc.ch == '}' || sc.ch == ':'
		|| (sc.ch == ',' && (IsADigit(sc.chNext) || (sc.chNext == '-' && IsADigit(sc.GetRelative(2)))));
}

void ColouriseVBDoc(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, LexerWordList keywordLists, Accessor &styler) {
	const WordList &keywords = keywordLists[0];
	const WordList &keywords2 = keywordLists[1];
	const WordList &keywords3 = keywordLists[2];
	const WordList &keywords4 = keywordLists[3];
	const WordList &keywords5 = keywordLists[4];
	const WordList &keywords6 = keywordLists[5];

	KeywordType kwType = KeywordType::None;
	int lineState = 0;
	int parenCount = 0;
	int fileNbDigits = 0;
	int visibleChars = 0;
	int chBefore = 0;
	int chPrevNonWhite = 0;
	int stylePrevNonWhite = SCE_VB_DEFAULT;
	bool isIfThenPreprocessor = false;
	bool isEndPreprocessor = false;
	std::vector<int> nestedState;

	const Language language = static_cast<Language>(styler.GetPropertyInt("lexer.lang"));
	if (startPos != 0) {
		// backtrack to the line starts expression inside interpolated string literal.
		BacktrackToStart(styler, VBLineStateStringInterpolation, startPos, lengthDoc, initStyle);
	}

	StyleContext sc(startPos, lengthDoc, initStyle, styler);
	if (sc.currentLine > 0) {
		lineState = styler.GetLineState(sc.currentLine - 1);
		parenCount = lineState >> 16;
		lineState &= VBLineStateLineContinuation;
	}
	if (startPos != 0 && IsSpaceEquiv(initStyle)) {
		LookbackNonWhite(styler, startPos, SCE_VB_LINE_CONTINUATION, chPrevNonWhite, stylePrevNonWhite);
	}

	while (sc.More()) {
		switch (sc.state) {
		case SCE_VB_OPERATOR:
		case SCE_VB_OPERATOR2:
		case SCE_VB_LINE_CONTINUATION:
			sc.SetState(SCE_VB_DEFAULT);
			break;

		case SCE_VB_IDENTIFIER:
			if (!IsIdentifierCharEx(sc.ch)) {
				// In Basic (except VBScript), a variable name or a function name
				// can end with a special character indicating the type of the value
				// held or returned.
				bool skipType = false;
				if (sc.ch == ']' || (language != Language::VBScript && IsTypeCharacter(sc.ch))) {
					skipType = sc.ch != ']';
					++visibleChars; // bracketed [keyword] identifier
					sc.Forward();
				}
				char s[64];
				sc.GetCurrentLowered(s, sizeof(s));
				const Sci_Position len = sc.LengthCurrent();
				if (skipType && len == 4) { // for type character after `rem`
					s[len - 1] = '\0';
				}
				if (StrEqual(s, "rem")) {
					sc.ChangeState(SCE_VB_COMMENTLINE);
				} else {
					if (!skipType) {
						const int chNext = sc.GetLineNextChar();
						if (s[0] == '[') {
							if (visibleChars == len && chNext == ':') {
								sc.ChangeState(SCE_VB_LABEL);
							}
						} else if ((isIfThenPreprocessor && StrEqual(s, "then")) || (isEndPreprocessor
							&& StrEqualsAny(s, "if", "region", "externalsource"))) {
							sc.ChangeState(SCE_VB_PREPROCESSOR);
						} else if (keywords.InList(s)) {
							sc.ChangeState(SCE_VB_KEYWORD3);
							if (chBefore != '.' && parenCount == 0) {
								sc.ChangeState(SCE_VB_KEYWORD);
								if (StrEqual(s, "if")) {
									if (language == Language::VBNET && visibleChars > 2 && chNext == '(') {
										sc.ChangeState(SCE_VB_KEYWORD3); // If operator
									}
								} else if (StrEqual(s, "dim")) {
									lineState = VBLineType_DimLine;
								} else if (StrEqual(s, "const")) {
									lineState = VBLineType_ConstLine;
								} else if (StrEqual(s, "type")) {
									if (visibleChars == len || kwType == KeywordType::AccessModifier) {
										lineState = VBLineType_VB6TypeLine;
									}
								} else if (StrEqual(s, "end")) {
									kwType = KeywordType::End;
								} else if (StrEqualsAny(s, "sub", "function")) {
									if (kwType != KeywordType::End) {
										kwType = KeywordType::Function;
									}
								} else if (StrEqualsAny(s, "public", "protected", "private", "friend")) {
									kwType = KeywordType::AccessModifier;
								}
							}
						} else if (keywords2.InList(s)) {
							sc.ChangeState(SCE_VB_KEYWORD2);
						} else if (visibleChars == len && chNext == ':') {
							sc.ChangeState(SCE_VB_LABEL);
						} else if (keywords3.InList(s)) {
							sc.ChangeState(SCE_VB_KEYWORD3);
						} else if (language != Language::VBScript && s[0] == '#' && keywords4.InList(s + 1)) {
							sc.ChangeState(SCE_VB_PREPROCESSOR);
							isIfThenPreprocessor = StrEqualsAny(s, "#if", "#elseif");
							isEndPreprocessor = StrEqual(s, "#end");
						} else if (keywords5.InList(s)) {
							sc.ChangeState(SCE_VB_ATTRIBUTE);
						} else if (keywords6.InList(s)) {
							sc.ChangeState(SCE_VB_CONSTANT);
						} else if (kwType == KeywordType::Function) {
							sc.ChangeState(SCE_VB_FUNCTION_DEFINITION);
						}
						stylePrevNonWhite = sc.state;
						if (sc.state != SCE_VB_KEYWORD) {
							kwType = KeywordType::None;
						}
					}
					sc.SetState(SCE_VB_DEFAULT);
				}
			}
			break;

		case SCE_VB_NUMBER:
			if (!IsVBNumber(sc.ch, sc.chPrev)) {
				sc.SetState(SCE_VB_DEFAULT);
			}
			break;

		case SCE_VB_STRING:
		case SCE_VB_INTERPOLATED_STRING:
			if (sc.atLineStart && language != Language::VBNET) {
				// multiline since VB.NET 14
				sc.SetState(SCE_VB_DEFAULT);
			} else if (sc.ch == '\"') {
				if (sc.chNext == '\"') {
					sc.Forward();
				} else {
					if (sc.chNext == 'c' || sc.chNext == 'C' || sc.chNext == '$') {
						sc.Forward();
					}
					sc.ForwardSetState(SCE_VB_DEFAULT);
				}
			} else if (sc.state == SCE_VB_INTERPOLATED_STRING) {
				if (sc.ch == '{') {
					if (sc.chNext == '{') {
						sc.Forward();
					} else {
						++parenCount;
						nestedState.push_back(0);
						sc.SetState(SCE_VB_OPERATOR2);
						sc.ForwardSetState(SCE_VB_DEFAULT);
					}
				} else if (sc.ch == '}') {
					if (!nestedState.empty()) {
						--parenCount;
						nestedState.pop_back();
						sc.SetState(SCE_VB_OPERATOR2);
						sc.ForwardSetState(SCE_VB_INTERPOLATED_STRING);
						continue;
					}
					if (sc.chNext == '}') {
						sc.Forward();
					}
				}
			}
			break;

		case SCE_VB_COMMENTLINE:
			if (sc.atLineStart) {
				if (lineState == VBLineStateLineContinuation) {
					lineState = VBLineType_CommentLine;
				} else {
					sc.SetState(SCE_VB_DEFAULT);
				}
			} else if (language == Language::VBA && sc.ch == '_' && sc.chPrev <= ' ') {
				if (sc.GetLineNextChar(true) == '\0') {
					lineState |= VBLineStateLineContinuation;
					sc.SetState(SCE_VB_LINE_CONTINUATION);
					sc.ForwardSetState(SCE_VB_COMMENTLINE);
				}
			}
			break;

		case SCE_VB_FILENUMBER:
			if (IsADigit(sc.ch)) {
				fileNbDigits++;
				if (fileNbDigits > 3) {
					sc.ChangeState(SCE_VB_DATE);
				}
			} else if (sc.ch == '\r' || sc.ch == '\n' || sc.ch == ',') {
				// Regular uses: Close #1; Put #1, ...; Get #1, ... etc.
				// Too bad if date is format #27, Oct, 2003# or something like that...
				// Use regular number state
				sc.ChangeState(SCE_VB_NUMBER);
				sc.SetState(SCE_VB_DEFAULT);
			} else if (sc.ch == '#') {
				sc.ChangeState(SCE_VB_DATE);
				sc.ForwardSetState(SCE_VB_DEFAULT);
			} else {
				sc.ChangeState(SCE_VB_DATE);
			}
			if (sc.state != SCE_VB_FILENUMBER) {
				fileNbDigits = 0;
			}
			break;

		case SCE_VB_DATE:
			if (sc.atLineStart) {
				sc.SetState(SCE_VB_DEFAULT);
			} else if (sc.ch == '#') {
				sc.ForwardSetState(SCE_VB_DEFAULT);
			}
			break;

		case SCE_VB_FORMAT_SPECIFIER:
			if (IsInvalidFormatSpecifier(sc.ch)) {
				sc.SetState(SCE_VB_INTERPOLATED_STRING);
				continue;
			}
			break;
		}

		if (sc.state == SCE_VB_DEFAULT) {
			if (sc.ch == '\'') {
				sc.SetState(SCE_VB_COMMENTLINE);
				if (visibleChars == 0) {
					lineState = VBLineType_CommentLine;
				}
			} else if (sc.ch == '\"') {
				sc.SetState(SCE_VB_STRING);
			} else if (language == Language::VBNET && sc.Match('$', '"')) {
				sc.SetState(SCE_VB_INTERPOLATED_STRING);
				sc.Forward();
			} else if (sc.ch == '#') {
				const int chNext = UnsafeLower(sc.chNext);
				if (chNext == 'e' || chNext == 'i' || chNext == 'r' || chNext == 'c')
					sc.SetState(SCE_VB_IDENTIFIER);
				else
					sc.SetState(SCE_VB_FILENUMBER);
			} else if (sc.ch == '&' && IsVBNumberPrefix(sc.chNext) && !PreferStringConcat(chPrevNonWhite, stylePrevNonWhite)) {
				sc.SetState(SCE_VB_NUMBER);
				sc.Forward();
			} else if (IsNumberStart(sc.ch, sc.chNext)) {
				sc.SetState(SCE_VB_NUMBER);
			} else if (sc.ch == '_' && sc.chNext <= ' '/* && (sc.chPrev <= ' ' || language == Language::VBScript)*/) {
				sc.SetState(SCE_VB_LINE_CONTINUATION);
			} else if (IsIdentifierStartEx(sc.ch) || sc.ch == '[') { // bracketed [keyword] identifier
				chBefore = chPrevNonWhite;
				sc.SetState(SCE_VB_IDENTIFIER);
			} else if (IsAGraphic(sc.ch)) {
				sc.SetState(SCE_VB_OPERATOR);
				if (nestedState.empty()) {
					if (sc.ch == '(') {
						++parenCount;
					} else if (sc.ch == ')' && parenCount > 0) {
						--parenCount;
					}
				} else {
					sc.ChangeState(SCE_VB_OPERATOR2);
					if (sc.ch == '(') {
						nestedState.back() += 1;
					} else if (sc.ch == ')') {
						nestedState.back() -= 1;
					}
					if (nestedState.back() <= 0 && IsInterpolatedStringEnd(sc)) {
						sc.ChangeState((sc.ch == '}') ? SCE_VB_INTERPOLATED_STRING : SCE_VB_FORMAT_SPECIFIER);
						continue;
					}
				}
			}
		}

		if (!isspacechar(sc.ch)) {
			visibleChars++;
			if (!IsSpaceEquiv(sc.state)) {
				chPrevNonWhite = sc.ch;
				stylePrevNonWhite = sc.state;
			}
		}
		if (sc.atLineEnd) {
			if (!nestedState.empty()) {
				lineState |= VBLineStateStringInterpolation;
			}
			styler.SetLineState(sc.currentLine, lineState | (parenCount << 16));
			lineState &= VBLineStateLineContinuation;
			isIfThenPreprocessor = false;
			isEndPreprocessor = false;
			visibleChars = 0;
			kwType = KeywordType::None;
		}
		sc.Forward();
	}

	sc.Complete();
}

bool VBMatchNextWord(LexAccessor &styler, Sci_Position startPos, Sci_Position endPos, const char *word) noexcept {
	const Sci_Position pos = LexSkipSpaceTab(styler, startPos, endPos);
	return isspacechar(LexCharAt(pos + static_cast<int>(strlen(word))))
		&& styler.MatchLowerCase(pos, word);
}
int IsVBProperty(LexAccessor &styler, Sci_Line line, Sci_Position startPos) noexcept {
	const Sci_Position endPos = styler.LineStart(line + 1) - 1;
	bool visibleChars = false;
	for (Sci_Position i = startPos; i < endPos; i++) {
		const uint8_t ch = UnsafeLower(styler[i]);
		const int style = styler.StyleAt(i);
		if (style == SCE_VB_OPERATOR && ch == '(') {
			return true;
		}
		if (style == SCE_VB_KEYWORD && !visibleChars
			&& (ch == 'g' || ch == 'l' || ch == 's')
			&& UnsafeLower(styler[i + 1]) == 'e'
			&& UnsafeLower(styler[i + 2]) == 't'
			&& isspacechar(styler[i + 3])) {
			return 2;
		}
		if (ch > ' ') {
			visibleChars = true;
		}
	}
	return false;
}

#define VBMatch(word)			styler.MatchLowerCase(i, word)
#define VBMatchNext(pos, word)	VBMatchNextWord(styler, pos, endPos, word)

struct FoldLineState {
	int lineState;
	constexpr explicit FoldLineState(int lineState_) noexcept : lineState(lineState_) {}
	int GetLineType() const noexcept {
		return lineState & 3;
	}
};

void FoldVBDoc(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, LexerWordList /*keywordLists*/, Accessor &styler) {
	const Sci_PositionU endPos = startPos + lengthDoc;
	Sci_Line lineCurrent = styler.GetLine(startPos);
	FoldLineState foldPrev(0);
	int levelCurrent = SC_FOLDLEVELBASE;
	if (lineCurrent > 0) {
		levelCurrent = styler.LevelAt(lineCurrent - 1) >> 16;
		foldPrev = FoldLineState(styler.GetLineState(lineCurrent - 1));
	}

	int levelNext = levelCurrent;
	FoldLineState foldCurrent(styler.GetLineState(lineCurrent));
	Sci_PositionU lineStartNext = styler.LineStart(lineCurrent + 1);
	lineStartNext = sci::min(lineStartNext, endPos);

	int style = initStyle;
	int styleNext = styler.StyleAt(startPos);

	int visibleChars = 0;
	int numBegin = 0;		// nested Begin ... End, found in VB6 Form
	bool isEnd = false;		// End {Function Sub}{If}{Class Module Structure Interface Operator Enum}{Property Event}{Type}
	bool isInterface = false;// {Property Function Sub Event Interface Class Structure }
	bool isProperty = false;// Property: Get Set
	bool isCustom = false;	// Custom Event
	bool isExit = false;	// Exit {Function Sub Property}
	bool isDeclare = false;	// Declare, Delegate {Function Sub}
	bool isIf = false;		// If ... Then \r\n ... \r\n End If
	static Sci_Line lineIf = 0;
	static Sci_Line lineThen = 0;

	while (startPos < endPos) {
		const Sci_PositionU i = startPos;
		const int stylePrev = style;
		style = styleNext;
		const char ch = styler[startPos];
		styleNext = styler.StyleAt(++startPos);

		if (style == SCE_VB_KEYWORD && stylePrev != SCE_VB_KEYWORD) { // not a member, not bracketed [keyword] identifier
			if (visibleChars == 0 && (VBMatch("for") || (VBMatch("do") && isspacechar(LexCharAt(i + 2))) // not Double
				|| VBMatch("while") || (VBMatch("try") && isspacechar(LexCharAt(i + 3))) // not TryCast
				|| (VBMatch("select") && VBMatchNext(i + 6, "case")) // Select Case
				|| (VBMatch("with") && isspacechar(LexCharAt(i + 4))) // not WithEvents, not With {...}
				|| VBMatch("namespace") || VBMatch("synclock") || VBMatch("using")
				|| (isProperty && (VBMatch("set") || (VBMatch("get") && isspacechar(LexCharAt(i + 3))))) // not GetType
				|| (isCustom && (VBMatch("raiseevent") || VBMatch("addhandler") || VBMatch("removehandler")))
				)) {
				levelNext++;
			} else if (visibleChars == 0 && (VBMatch("next") || VBMatch("loop") || VBMatch("wend"))) {
				levelNext--;
			} else if (VBMatch("exit") && (VBMatchNext(i + 4, "function") || VBMatchNext(i + 4, "sub")
				|| VBMatchNext(i + 4, "property")
				)) {
				isExit = true;
			} else if (VBMatch("begin")) {
				levelNext++;
				if (isspacechar(LexCharAt(i + 5)))
					numBegin++;
			} else if (VBMatch("end")) {
				levelNext--;
				int chEnd = static_cast<unsigned char>(LexCharAt(i + 3));
				if (chEnd == ' ' || chEnd == '\t') {
					const Sci_Position pos = LexSkipSpaceTab(styler, i + 3, endPos);
					chEnd = static_cast<unsigned char>(LexCharAt(pos));
					// check if End is used to terminate statement
					if (IsAlpha(chEnd) && (VBMatchNext(pos, "function") || VBMatchNext(pos, "sub")
						|| VBMatchNext(pos, "if") || VBMatchNext(pos, "class") || VBMatchNext(pos, "structure")
						|| VBMatchNext(pos, "module") || VBMatchNext(pos, "enum") || VBMatchNext(pos, "interface")
						|| VBMatchNext(pos, "operator") || VBMatchNext(pos, "property") || VBMatchNext(pos, "event")
						|| VBMatchNext(pos, "type") // VB6
						)) {
						isEnd = true;
					}
				}
				if (chEnd == '\r' || chEnd == '\n' || chEnd == '\'') {
					isEnd = false;
					if (numBegin == 0)	levelNext++;// End can be placed anywhere, but not used to terminate statement
					if (numBegin > 0)	numBegin--;
				}
				if (VBMatch("endif")) // same as End If
					isIf = false;
				// one line: If ... Then ... End If
				if (lineCurrent == lineIf && lineCurrent == lineThen)
					levelNext++;
			} else if (VBMatch("if")) {
				isIf = true;
				lineIf = lineCurrent;
				if (isEnd) {
					isEnd = false; isIf = false;
				} else				levelNext++;
			} else if (VBMatch("then")) {
				if (isIf) {
					isIf = false;
					const Sci_Position pos = LexSkipSpaceTab(styler, i + 4, endPos);
					const char chEnd = LexCharAt(pos);
					if (!(chEnd == '\r' || chEnd == '\n' || chEnd == '\''))
						levelNext--;
				}
				lineThen = lineCurrent;
			} else if ((!isInterface && (VBMatch("class") || VBMatch("structure")))
				|| VBMatch("module") || VBMatch("enum") || VBMatch("operator")
				) {
				if (isEnd)			isEnd = false;
				else				levelNext++;
			} else if (VBMatch("interface")) {
				if (!(isEnd || isInterface))
					levelNext++;
				isInterface = true;
				if (isEnd) {
					isEnd = false; isInterface = false;
				}
			} else if (VBMatch("declare") || VBMatch("delegate")) {
				isDeclare = true;
			} else if (!isInterface && (VBMatch("sub") || VBMatch("function"))) {
				if (!(isEnd || isExit || isDeclare))
					levelNext++;
				if (isEnd)			isEnd = false;
				if (isExit)			isExit = false;
				if (isDeclare)		isDeclare = false;
			} else if (!isInterface && VBMatch("property")) {
				isProperty = true;
				if (!(isEnd || isExit)) {
					const int result = IsVBProperty(styler, lineCurrent, i + 8);
					levelNext += result != 0;
					isProperty = result & true;
				}
				if (isEnd) {
					isEnd = false; isProperty = false;
				}
				if (isExit)			isExit = false;
			} else if (VBMatch("custom")) {
				isCustom = true;
			} else if (!isInterface && isCustom && VBMatch("event")) {
				if (isEnd) {
					isEnd = false; isCustom = false;
				} else 				levelNext++;
			} else if (VBMatch("type") && isspacechar(LexCharAt(i + 4))) { // not TypeOf, VB6: [...] Type ... End Type
				if (!isEnd && (foldCurrent.lineState & VBLineType_VB6TypeLine) != 0)
					levelNext++;
				if (isEnd)	isEnd = false;
			}
		}
		else if (style == SCE_VB_PREPROCESSOR) {
			if (VBMatch("#if") || VBMatch("#region") || VBMatch("#externalsource"))
				levelNext++;
			else if (VBMatch("#end"))
				levelNext--;
		}
		else if (style == SCE_VB_OPERATOR) {
			// Anonymous With { ... }
			if (AnyOf<'{', '}'>(ch)) {
				levelNext += ('{' + '}')/2 - ch;
			}
		}

		if (visibleChars == 0 && !isspacechar(ch)) {
			visibleChars++;
		}
		if (startPos == lineStartNext) {
			const FoldLineState foldNext(styler.GetLineState(lineCurrent + 1));
			levelNext = sci::max(levelNext, SC_FOLDLEVELBASE);
			if (foldCurrent.GetLineType() != 0) {
				if (foldCurrent.GetLineType() != foldPrev.GetLineType()) {
					levelNext++;
				}
				if (foldCurrent.GetLineType() != foldNext.GetLineType()) {
					levelNext--;
				}
			}

			const int levelUse = levelCurrent;
			int lev = levelUse | (levelNext << 16);
			if (levelUse < levelNext) {
				lev |= SC_FOLDLEVELHEADERFLAG;
			}
			styler.SetLevel(lineCurrent, lev);

			lineCurrent++;
			lineStartNext = styler.LineStart(lineCurrent + 1);
			lineStartNext = sci::min(lineStartNext, endPos);
			levelCurrent = levelNext;
			foldPrev = foldCurrent;
			foldCurrent = foldNext;
			visibleChars = 0;
		}
	}
}

}

extern const LexerModule lmVisualBasic(SCLEX_VISUALBASIC, ColouriseVBDoc, "vb", FoldVBDoc);
