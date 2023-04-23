/**
 * src/common/visitor/clang_utils.cc
 *
 * Copyright (c) 2021-2023 Bartek Kryza <bkryza@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "clang_utils.h"

#include <clang/Lex/Preprocessor.h>

namespace clanguml::common {

model::access_t access_specifier_to_access_t(
    clang::AccessSpecifier access_specifier)
{
    auto access = model::access_t::kPublic;
    switch (access_specifier) {
    case clang::AccessSpecifier::AS_public:
        access = model::access_t::kPublic;
        break;
    case clang::AccessSpecifier::AS_private:
        access = model::access_t::kPrivate;
        break;
    case clang::AccessSpecifier::AS_protected:
        access = model::access_t::kProtected;
        break;
    default:
        break;
    }

    return access;
}

model::namespace_ get_tag_namespace(const clang::TagDecl &declaration)
{
    model::namespace_ ns;

    const auto *parent{declaration.getParent()};

    // First walk up to the nearest namespace, e.g. from nested class or enum
    while ((parent != nullptr) && !parent->isNamespace()) {
        parent = parent->getParent();
    }

    // Now build up the namespace
    std::deque<std::string> namespace_tokens;
    while ((parent != nullptr) && parent->isNamespace()) {
        if (const auto *ns_decl = clang::dyn_cast<clang::NamespaceDecl>(parent);
            ns_decl != nullptr) {
            if (!ns_decl->isInline() && !ns_decl->isAnonymousNamespace())
                namespace_tokens.push_front(ns_decl->getNameAsString());
        }

        parent = parent->getParent();
    }

    for (const auto &ns_token : namespace_tokens) {
        ns |= ns_token;
    }

    return ns;
}

model::namespace_ get_template_namespace(const clang::TemplateDecl &declaration)
{
    model::namespace_ ns{declaration.getQualifiedNameAsString()};
    ns.pop_back();

    return ns;
}

std::string get_tag_name(const clang::TagDecl &declaration)
{
    auto base_name = declaration.getNameAsString();
    if (base_name.empty()) {
        base_name =
            fmt::format("(anonymous_{})", std::to_string(declaration.getID()));
    }

    if ((declaration.getParent() != nullptr) &&
        declaration.getParent()->isRecord()) {
        // If the record is nested within another record (e.g. class or struct)
        // we have to maintain a containment namespace in order to ensure
        // unique names within the diagram
        std::deque<std::string> record_parent_names;
        record_parent_names.push_front(base_name);

        const auto *cls_parent{declaration.getParent()};
        while (cls_parent->isRecord()) {
            if (const auto *record_decl =
                    clang::dyn_cast<clang::RecordDecl>(cls_parent);
                record_decl != nullptr) {
                record_parent_names.push_front(record_decl->getNameAsString());
            }
            cls_parent = cls_parent->getParent();
        }
        return fmt::format("{}", fmt::join(record_parent_names, "##"));
    }

    return base_name;
}

std::string to_string(const clang::QualType &type, const clang::ASTContext &ctx,
    bool try_canonical)
{
    clang::PrintingPolicy print_policy(ctx.getLangOpts());
    print_policy.SuppressScope = false;
    print_policy.PrintCanonicalTypes = false;

    std::string result;

    result = type.getAsString(print_policy);

    if (try_canonical && result.find('<') != std::string::npos) {
        auto canonical_type_name =
            type.getCanonicalType().getAsString(print_policy);

        auto result_qualified_template_name =
            result.substr(0, result.find('<'));
        auto result_template_arguments = result.substr(result.find('<'));

        auto canonical_qualified_template_name =
            canonical_type_name.substr(0, canonical_type_name.find('<'));

        // Choose the longer name (why do I have to do this?)
        if (result_qualified_template_name.size() <
            canonical_qualified_template_name.size()) {

            result =
                canonical_qualified_template_name + result_template_arguments;
        }
    }

    // If for any reason clang reports the type as empty string, make sure
    // it has some default name
    if (result.empty())
        result = "(anonymous)";
    else if (util::contains(result, "unnamed struct") ||
        util::contains(result, "unnamed union")) {
        result = common::get_tag_name(*type->getAsTagDecl());
    }
    else if (util::contains(result, "anonymous struct") ||
        util::contains(result, "anonymous union")) {
        result = common::get_tag_name(*type->getAsTagDecl());
    }

    // Remove trailing spaces after commas in template arguments
    clanguml::util::replace_all(result, ", ", ",");
    clanguml::util::replace_all(result, "> >", ">>");

    return result;
}

std::string to_string(const clang::RecordType &type,
    const clang::ASTContext &ctx, bool try_canonical)
{
    return to_string(type.desugar(), ctx, try_canonical);
}

std::string to_string(
    const clang::TemplateArgument &arg, const clang::ASTContext *ctx)
{
    switch (arg.getKind()) {
    case clang::TemplateArgument::Expression:
        return to_string(arg.getAsExpr());
    case clang::TemplateArgument::Type:
        return to_string(arg.getAsType(), *ctx, false);
    case clang::TemplateArgument::Null:
        return "";
    case clang::TemplateArgument::NullPtr:
        return "nullptr";
    case clang::TemplateArgument::Integral:
        return std::to_string(arg.getAsIntegral().getExtValue());
    default:
        return "";
    }
}

std::string to_string(const clang::Expr *expr)
{
    const clang::LangOptions lang_options;
    std::string result;
    llvm::raw_string_ostream ostream(result);
    expr->printPretty(ostream, nullptr, clang::PrintingPolicy(lang_options));

    return result;
}

std::string to_string(const clang::Stmt *stmt)
{
    const clang::LangOptions lang_options;
    std::string result;
    llvm::raw_string_ostream ostream(result);
    stmt->printPretty(ostream, nullptr, clang::PrintingPolicy(lang_options));

    return result;
}

std::string to_string(const clang::FunctionTemplateDecl *decl)
{
    std::vector<std::string> template_parameters;
    // Handle template function
    for (const auto *parameter : *decl->getTemplateParameters()) {
        if (clang::dyn_cast_or_null<clang::TemplateTypeParmDecl>(parameter) !=
            nullptr) {
            const auto *template_type_parameter =
                clang::dyn_cast_or_null<clang::TemplateTypeParmDecl>(parameter);

            std::string template_parameter{
                template_type_parameter->getNameAsString()};

            if (template_type_parameter->isParameterPack())
                template_parameter += "...";

            template_parameters.emplace_back(std::move(template_parameter));
        }
        else {
            // TODO
        }
    }
    return fmt::format("{}<{}>({})", decl->getQualifiedNameAsString(),
        fmt::join(template_parameters, ","), "");
}

std::string to_string(const clang::TypeConstraint *tc)
{
    if (tc == nullptr)
        return {};

    const clang::PrintingPolicy print_policy(
        tc->getNamedConcept()->getASTContext().getLangOpts());

    std::string ostream_buf;
    llvm::raw_string_ostream ostream{ostream_buf};
    tc->print(ostream, print_policy);

    return ostream.str();
}

std::string get_source_text_raw(
    clang::SourceRange range, const clang::SourceManager &sm)
{
    return clang::Lexer::getSourceText(
        clang::CharSourceRange::getCharRange(range), sm, clang::LangOptions())
        .str();
}

std::string get_source_text(
    clang::SourceRange range, const clang::SourceManager &sm)
{
    const clang::LangOptions lo;

    auto start_loc = sm.getSpellingLoc(range.getBegin());
    auto last_token_loc = sm.getSpellingLoc(range.getEnd());
    auto end_loc = clang::Lexer::getLocForEndOfToken(last_token_loc, 0, sm, lo);
    auto printable_range = clang::SourceRange{start_loc, end_loc};
    return get_source_text_raw(printable_range, sm);
}

std::tuple<unsigned int, unsigned int, std::string>
extract_template_parameter_index(const std::string &type_parameter)
{
    assert(type_parameter.find("type-parameter-") == 0);

    auto type_parameter_and_suffix = util::split(type_parameter, " ");

    auto toks = util::split(
        type_parameter_and_suffix.front().substr(strlen("type-parameter-")),
        "-");

    std::string qualifier;

    if (type_parameter_and_suffix.size() > 1) {
        qualifier = type_parameter_and_suffix.at(1);
    }

    return {std::stoi(toks.at(0)), std::stoi(toks.at(1)), std::move(qualifier)};
}

bool is_subexpr_of(const clang::Stmt *parent_stmt, const clang::Stmt *sub_stmt)
{
    if (parent_stmt == nullptr || sub_stmt == nullptr)
        return false;

    if (parent_stmt == sub_stmt)
        return true;

    return std::any_of(parent_stmt->child_begin(), parent_stmt->child_end(),
        [sub_stmt](const auto *e) { return is_subexpr_of(e, sub_stmt); });
}

template <> id_t to_id(const std::string &full_name)
{
    return static_cast<id_t>(std::hash<std::string>{}(full_name) >> 3U);
}

template <> id_t to_id(const clang::NamespaceDecl &declaration)
{
    return to_id(get_qualified_name(declaration));
}

template <> id_t to_id(const clang::RecordDecl &declaration)
{
    return to_id(get_qualified_name(declaration));
}

template <> id_t to_id(const clang::EnumDecl &declaration)
{
    return to_id(get_qualified_name(declaration));
}

template <> id_t to_id(const clang::TagDecl &declaration)
{
    return to_id(get_qualified_name(declaration));
}

template <> id_t to_id(const clang::CXXRecordDecl &declaration)
{
    return to_id(get_qualified_name(declaration));
}

template <> id_t to_id(const clang::EnumType &t) { return to_id(*t.getDecl()); }

template <> id_t to_id(const std::filesystem::path &file)
{
    return to_id(file.lexically_normal().string());
}

template <> id_t to_id(const clang::TemplateArgument &template_argument)
{
    if (template_argument.getKind() == clang::TemplateArgument::Type) {
        if (const auto *enum_type =
                template_argument.getAsType()->getAs<clang::EnumType>();
            enum_type != nullptr)
            return to_id(*enum_type->getAsTagDecl());

        if (const auto *record_type =
                template_argument.getAsType()->getAs<clang::RecordType>();
            record_type != nullptr)
            return to_id(*record_type->getAsRecordDecl());
    }

    throw std::runtime_error("Cannot generate id for template argument");
}

std::pair<common::model::namespace_, std::string> split_ns(
    const std::string &full_name)
{
    assert(!full_name.empty());

    auto name_before_template = ::clanguml::util::split(full_name, "<")[0];
    auto ns = common::model::namespace_{
        ::clanguml::util::split(name_before_template, "::")};
    auto name = ns.name();
    ns.pop_back();
    return {ns, name};
}

std::vector<common::model::template_parameter> parse_unexposed_template_params(
    const std::string &params,
    const std::function<std::string(const std::string &)> &ns_resolve,
    int depth)
{
    using common::model::template_parameter;

    std::vector<template_parameter> res;

    auto it = params.begin();
    while (std::isspace(*it) != 0)
        ++it;

    std::string type{};
    std::vector<template_parameter> nested_params;
    bool complete_class_template_argument{false};

    while (it != params.end()) {
        if (*it == '<') {
            int nested_level{0};
            auto bracket_match_begin = it + 1;
            auto bracket_match_end = bracket_match_begin;
            while (bracket_match_end != params.end()) {
                if (*bracket_match_end == '<') {
                    nested_level++;
                }
                else if (*bracket_match_end == '>') {
                    if (nested_level > 0)
                        nested_level--;
                    else
                        break;
                }
                else {
                }
                bracket_match_end++;
            }

            std::string nested_params_str(
                bracket_match_begin, bracket_match_end);

            nested_params = parse_unexposed_template_params(
                nested_params_str, ns_resolve, depth + 1);

            if (nested_params.empty()) {
                // We couldn't extract any nested template parameters from
                // `nested_params_str` so just add it as type of template
                // argument as is
                nested_params.emplace_back(
                    template_parameter::make_unexposed_argument(
                        nested_params_str));
            }

            it = bracket_match_end - 1;
        }
        else if (*it == '>') {
            complete_class_template_argument = true;
            if (depth == 0) {
                break;
            }
        }
        else if (*it == ',') {
            complete_class_template_argument = true;
        }
        else {
            type += *it;
        }
        if (complete_class_template_argument) {
            auto t = template_parameter::make_unexposed_argument(
                ns_resolve(clanguml::util::trim_typename(type)));
            type = "";
            for (auto &&param : nested_params)
                t.add_template_param(std::move(param));

            res.emplace_back(std::move(t));
            complete_class_template_argument = false;
        }
        it++;
    }

    if (!type.empty()) {
        auto t = template_parameter::make_unexposed_argument(
            ns_resolve(clanguml::util::trim_typename(type)));
        type = "";
        for (auto &&param : nested_params)
            t.add_template_param(std::move(param));

        res.emplace_back(std::move(t));
    }

    return res;
}

bool is_type_parameter(const std::string &t)
{
    return t.find("type-parameter-") == 0;
}

bool is_qualifier(const std::string &q)
{
    return q == "&" || q == "&&" || q == "const&";
}

bool is_bracket(const std::string &b)
{
    return b == "(" || b == ")" || b == "[" || b == "]";
}

bool is_identifier_character(char c) { return std::isalnum(c) || c == '_'; }

bool is_identifier(const std::string &t)
{
    return std::isalpha(t.at(0)) &&
        std::all_of(t.begin(), t.end(),
            [](const char c) { return is_identifier_character(c); });
}

bool is_keyword(const std::string &t)
{
    static std::vector<std::string> keywords{"alignas", "alignof", "asm",
        "auto", "bool", "break", "case", "catch", "char", "char16_t",
        "char32_t", "class", "concept", "const", "constexpr", "const_cast",
        "continue", "decltype", "default", "delete", "do", "double",
        "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false",
        "float", "for", "friend", "goto", "if", "inline", "int", "long",
        "mutable", "namespace", "new", "noexcept", "nullptr", "operator",
        "private", "protected", "public", "register", "reinterpret_cast",
        "return", "requires", "short", "signed", "sizeof", "static",
        "static_assert", "static_cast", "struct", "switch", "template", "this",
        "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
        "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t",
        "while"};

    return util::contains(keywords, t);
}

bool is_qualified_identifier(const std::string &t)
{
    return std::isalpha(t.at(0)) &&
        std::all_of(t.begin(), t.end(), [](const char c) {
            return is_identifier_character(c) || c == ':';
        });
}

bool is_type_token(const std::string &t)
{
    return is_type_parameter(t) ||
        (is_identifier(t) && !is_qualifier(t) && !is_bracket(t));
}

std::vector<std::string> tokenize_unexposed_template_parameter(
    const std::string &t)
{
    std::vector<std::string> result;

    auto spaced_out = util::split(t, " ");

    for (const auto &word : spaced_out) {
        if (is_qualified_identifier(word)) {
            if (word != "class" && word != "templated" && word != "struct")
                result.push_back(word);
            continue;
        }

        std::string tok;

        for (const char c : word) {
            if (c == '(' || c == ')' || c == '[' || c == ']') {
                if (!tok.empty())
                    result.push_back(tok);
                result.push_back(std::string{c});
                tok.clear();
            }
            else if (c == ':') {
                if (!tok.empty() && tok != ":") {
                    result.push_back(tok);
                    tok = ":";
                }
                else {
                    tok += ':';
                }
            }
            else if (c == ',') {
                if (!tok.empty()) {
                    result.push_back(tok);
                }
                result.push_back(",");
                tok.clear();
            }
            else if (c == '*') {
                if (!tok.empty()) {
                    result.push_back(tok);
                }
                result.push_back("*");
                tok.clear();
            }
            else if (c == '.') {
                // This can only be the case if we have a variadic template,
                // right?
                if (tok == "..") {
                    result.push_back("...");
                    tok.clear();
                }
                else if (tok == ".") {
                    tok = "..";
                }
                else if (!tok.empty()) {
                    result.push_back(tok);
                    tok = ".";
                }
            }
            else {
                tok += c;
            }
        }

        tok = util::trim(tok);

        if (!tok.empty()) {
            if (tok != "class" && tok != "typename" && word != "struct")
                result.push_back(tok);
            tok.clear();
        }
    }

    return result;
}

} // namespace clanguml::common
