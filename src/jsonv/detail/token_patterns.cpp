/** \file
 *  
 *  Copyright (c) 2014-2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/detail/token_patterns.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>

namespace jsonv
{
namespace detail
{

template <std::ptrdiff_t N>
static match_result match_literal(const char* begin, const char* end, const char (& literal)[N], std::size_t& length)
{
    for (length = 0; length < (N-1); ++length)
    {
        if (begin + length == end || begin[length] != literal[length])
            return match_result::unmatched;
    }
    return match_result::complete;
}

static match_result match_true(const char* begin, const char* end, token_kind& kind, std::size_t& length)
{
    kind = token_kind::boolean;
    return match_literal(begin, end, "true", length);
}

static match_result match_false(const char* begin, const char* end, token_kind& kind, std::size_t& length)
{
    kind = token_kind::boolean;
    return match_literal(begin, end, "false", length);
}

static match_result match_null(const char* begin, const char* end, token_kind& kind, std::size_t& length)
{
    kind = token_kind::null;
    return match_literal(begin, end, "null", length);
}

enum class match_number_state
{
    initial,
    leading_minus,
    leading_zero,
    integer,
    decimal,
    exponent,
    exponent_sign,
    complete,
};

static match_result match_number(const char* begin, const char* end, token_kind& kind, std::size_t& length)
{
    kind            = token_kind::number;
    length          = 0U;
    auto state      = match_number_state::initial;
    auto max_length = std::size_t(end - begin);

    auto current = [&] ()
                   {
                       if (length < max_length)
                           return begin[length];
                       else
                           return '\0';
                   };

    // Initial: behavior of parse branches from here
    switch (current())
    {
    case '-':
        ++length;
        state = match_number_state::leading_minus;
        break;
    case '0':
        ++length;
        state = match_number_state::leading_zero;
        break;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        ++length;
        state = match_number_state::integer;
        break;
    default:
        return match_result::unmatched;
    }

    // Leading '-'
    if (state == match_number_state::leading_minus)
    {
        switch (current())
        {
        case '0':
            ++length;
            state = match_number_state::leading_zero;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ++length;
            state = match_number_state::integer;
            break;
        default:
            return match_result::unmatched;
        }
    }

    // Leading '0' or "-0"
    if (state == match_number_state::leading_zero)
    {
        switch (current())
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            // NOTE: This behavior is incorrect according to strict JSON parsing. However, this library matches the
            // input as a number in case `parse_options::numbers::decimal` is used.
            ++length;
            state = match_number_state::integer;
            break;
        case '.':
            ++length;
            state = match_number_state::decimal;
            break;
        case 'e':
        case 'E':
            ++length;
            state = match_number_state::exponent;
            break;
        default:
            state = match_number_state::complete;
            break;
        }
    }

    // Have only seen integer values
    while (state == match_number_state::integer)
    {
        switch (current())
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ++length;
            break;
        case '.':
            ++length;
            state = match_number_state::decimal;
            break;
        case 'e':
        case 'E':
            ++length;
            state = match_number_state::exponent;
            break;
        default:
            state = match_number_state::complete;
            break;
        }
    }

    // Just saw a '.'
    if (state == match_number_state::decimal)
    {
        switch (current())
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ++length;
            break;
        default:
            return match_result::unmatched;
        }

        while (state == match_number_state::decimal)
        {
            switch (current())
            {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                ++length;
                break;
            case 'e':
            case 'E':
                ++length;
                state = match_number_state::exponent;
                break;
            default:
                state = match_number_state::complete;
                break;
            }
        }
    }

    // Just saw 'e' or 'E'
    if (state == match_number_state::exponent)
    {
        switch (current())
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ++length;
            break;
        case '+':
        case '-':
            ++length;
            state = match_number_state::exponent_sign;
            break;
        default:
            return match_result::unmatched;
        }
    }

    // Just saw "e-", "e+", "E-", or "E+"
    if (state == match_number_state::exponent_sign)
    {
        switch (current())
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ++length;
            state = match_number_state::exponent;
            break;
        default:
            return match_result::unmatched;
        }
    }

    while (state == match_number_state::exponent)
    {
        switch (current())
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ++length;
            break;
        default:
            state = match_number_state::complete;
            break;
        }
    }

    assert(state == match_number_state::complete);
    return match_result::complete;
}

static match_result match_string(const char* begin, const char* end, token_kind& kind, std::size_t& length)
{
    assert(*begin == '\"');
    
    kind = token_kind::string;
    length = 1;
    
    while (true)
    {
        if (begin + length == end)
            return match_result::unmatched;
        
        if (begin[length] == '\"')
        {
            ++length;
            return match_result::complete;
        }
        else if (begin[length] == '\\')
        {
            if (begin + length + 1 == end)
                return match_result::unmatched;
            else
                length += 2;
        }
        else
        {
            ++length;
        }
    }
}

static match_result match_simple_string(const char* begin, const char* end, token_kind& kind, std::size_t& length)
{
    kind            = token_kind::string;
    length          = 0U;
    auto max_length = std::size_t(end - begin);

    auto current = [&] ()
                   {
                       if (length < max_length)
                           return begin[length];
                       else
                           return '\0';
                   };

    // R"(^[a-zA-Z_$][a-zA-Z0-9_$]*)"
    char c = current();
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '_') || (c == '$'))
        ++length;
    else
        return match_result::unmatched;

    while (true)
    {
        c = current();
        if (c == '\0')
            return match_result::complete;
        else if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '_') || (c == '$') || ('0' <= c && c <= '9'))
            ++length;
        else
            return match_result::complete;
    }
}

static match_result match_whitespace(const char* begin, const char* end, token_kind& kind, std::size_t& length)
{
    kind = token_kind::whitespace;
    for (length = 0; begin != end; ++length, ++begin)
    {
        switch (*begin)
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            continue;
        default:
            return match_result::complete;
        }
    }
    return match_result::complete;
}

static match_result match_comment(const char* begin, const char* end, token_kind& kind, std::size_t& length)
{
    assert(*begin == '/');
    
    kind = token_kind::comment;
    if (std::distance(begin, end) == 1)
    {
        length = 1;
        return match_result::unmatched;
    }
    else if (begin[1] == '*')
    {
        bool saw_asterisk = false;
        for (length = 2, begin += 2; begin != end; ++length, ++begin)
        {
            if (*begin == '*')
            {
                saw_asterisk = true;
            }
            else if (saw_asterisk && *begin == '/')
            {
                ++length;
                return match_result::complete;
            }
            else
            {
                saw_asterisk = false;
            }
        }
        return match_result::unmatched;
    }
    else
    {
        length = 1;
        return match_result::unmatched;
    }
    
}

match_result attempt_match(const char* begin, const char* end, token_kind& kind, std::size_t& length)
{
    auto result = [&] (match_result r, token_kind kind_, std::size_t length_)
                  {
                      kind = kind_;
                      length = length_;
                      return r;
                  };
    
    if (begin == end)
    {
        return result(match_result::unmatched, token_kind::unknown, 0);
    }
    
    switch (*begin)
    {
    case '[': return result(match_result::complete, token_kind::array_begin,          1);
    case ']': return result(match_result::complete, token_kind::array_end,            1);
    case '{': return result(match_result::complete, token_kind::object_begin,         1);
    case '}': return result(match_result::complete, token_kind::object_end,           1);
    case ':': return result(match_result::complete, token_kind::object_key_delimiter, 1);
    case ',': return result(match_result::complete, token_kind::separator,            1);
    case 't': return match_true( begin, end, kind, length);
    case 'f': return match_false(begin, end, kind, length);
    case 'n': return match_null( begin, end, kind, length);
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return match_number(begin, end, kind, length);
    case '\"':
        return match_string(begin, end, kind, length);
    case ' ':
    case '\t':
    case '\n':
    case '\r':
        return match_whitespace(begin, end, kind, length);
    case '/':
        return match_comment(begin, end, kind, length);
    default:
        return result(match_result::unmatched, token_kind::unknown, 1);
    }
}

path_match_result path_match(string_view input, string_view& match_contents)
{
    if (input.length() < 2)
        return path_match_result::invalid;
    
    match_result result;
    token_kind kind;
    std::size_t length;
    
    switch (input.at(0))
    {
    case '.':
        result = match_simple_string(input.data() + 1, input.data() + input.size(), kind, length);
        if (result == match_result::complete)
        {
            match_contents = input.substr(0, length + 1);
            return path_match_result::simple_object;
        }
        else
        {
            return path_match_result::invalid;
        }
    case '[':
        result = attempt_match(input.data() + 1, input.data() + input.length(), kind, length);
        if (result == match_result::complete)
        {
            if (input.length() == length + 1 || input.at(1 + length) != ']')
                return path_match_result::invalid;
            if (kind != token_kind::string && kind != token_kind::number)
                return path_match_result::invalid;
            
            match_contents = input.substr(0, length + 2);
            return path_match_result::brace;
        }
        else
        {
            return path_match_result::invalid;
        }
    default:
        return path_match_result::invalid;
    }
}

}
}
