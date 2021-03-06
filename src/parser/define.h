#ifndef SJASMPLUS_PARSER_DEFINES_H
#define SJASMPLUS_PARSER_DEFINES_H

#include <string>

#include "macro.h"

namespace parser {

struct DefineSp : RequiredNothing1L {};

struct DefineArg : until<at<TrailingNothing> > {};

struct Define : seq<if_must<TAO_PEGTL_ISTRING("DEFINE"), DefineSp, Identifier> ,
        opt<RequiredNothing1L, DefineArg> > {};

struct DefArrayArgList : MacroArgList {};

struct DefArraySp1 : RequiredNothing1L {};

struct DefArraySp2 : RequiredNothing1L {};

struct DefArray : if_must<TAO_PEGTL_ISTRING("DEFARRAY"), DefArraySp1,
        Identifier, DefArraySp2, DefArrayArgList> {};

template<> const std::string Ctrl<DefineSp>::ErrMsg;
template<> const std::string Ctrl<DefArrayArgList>::ErrMsg;
template<> const std::string Ctrl<DefArraySp1>::ErrMsg;
template<> const std::string Ctrl<DefArraySp2>::ErrMsg;

template<>
struct Actions<Define> {
    template<typename Input>
    static void apply(const Input &In, State &S) {
        S.Asm.Defines.set(S.Id, S.StringVec.empty() ? ""s : S.StringVec[0]);
    }
};

template<>
struct Actions<DefineArg> {
    template<typename Input>
    static void apply(const Input &In, State &S) {
        S.StringVec = {1, In.string()};
    }
};


template<>
struct Actions<DefArray> {
    template<typename Input>
    static void apply(const Input &In, State &S) {
        S.Asm.Defines.setArray(S.Id, S.StringVec);
    }
};

} // namespace parser

#endif //SJASMPLUS_PARSER_DEFINES_H
