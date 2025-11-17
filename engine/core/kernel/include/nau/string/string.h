// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
// nau/service/string.h


#pragma once
/* Prototype header for string type. Currently just EASTL.
 *
 * Defines nau::string, nau::string_view, nau::format
 *
 * */

#include <EASTL/string.h>
#include <fcntl.h>
#include <io.h>

#include <functional>
#include <iostream>
#include <string>

// clang-format off
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunqualified-std-cast-call"
#endif

#include "tinyutf8/tinyutf8.h"
#include "utf8/cpp20.h"
#include "nau/kernel/kernel_config.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif
// clang-format on

#include "nau/diag/assertion.h"
#include "nau/string/format.h"

namespace nau
{
    class string_view;

    /** Deprecated **/
    class string
    {
    public:
        using allocator_type = typename std::allocator<char>;
        using data_type = char8_t;
        using container_type = tiny_utf8::basic_string<char32_t, data_type, allocator_type>;

        using size_type = container_type::size_type;
        using difference_type = container_type::difference_type;
        using value_type = container_type::value_type;
        using reference = container_type::reference;
        using checked_reference = container_type::checked_reference;
        using raw_reference = container_type::raw_reference;
        using raw_checked_reference = container_type::raw_checked_reference;
        using const_reference = container_type::const_reference;
        using width_type = container_type::width_type;
        using iterator = container_type::iterator;
        using const_iterator = container_type::const_iterator;
        using reverse_iterator = container_type::reverse_iterator;
        using const_reverse_iterator = container_type::const_reverse_iterator;
        using raw_iterator = container_type::raw_iterator;
        using raw_const_iterator = container_type::raw_const_iterator;
        using raw_reverse_iterator = container_type::raw_reverse_iterator;
        using raw_const_reverse_iterator = container_type::raw_const_reverse_iterator;
        using allocator_type = container_type::allocator_type;
        using indicator_type = container_type::indicator_type;
        enum : size_type{													npos = (size_type)-1 };

        container_type m_data;

#pragma region default_constructors
    private:
        template <class T>
            requires std::is_same_v<std::remove_cvref_t<T>, container_type>
        string(T&& m_data) :
            m_data(std::forward<T>(m_data)){};

    public:
        string() = default;

        string(const string&) = default;
        string(string&&) = default;
        string& operator=(const string&) = default;
        string& operator=(string&&) = default;

        string(size_t n) :
            m_data(n, u8'\0'){};

#pragma endregion

#pragma region locale_string_constructors
        template <class S>
            requires std::is_constructible_v<eastl::string_view, S>
        string(S&& s)  // TODO locale
        {
            auto sPtr = eastl::string_view(s).data();
            if (!sPtr)
            {
                m_data = u8"";
                return;
            }
            if(utf8::is_valid((const char8_t*)sPtr))
            {
                m_data = (const char8_t*)sPtr;
            }
            else
            {
                NAU_FAILURE(u8"Invalid input string.");
                m_data.clear();
            }
        }

        template <class S>
            requires(!std::is_constructible_v<eastl::string_view, S>) &&
                    std::is_constructible_v<std::string_view, S>
        string(S&& s)  // TODO locale
        {
            auto sPtr = std::string_view(s).data();
            if(!sPtr)
            {
                m_data = u8"";
                return;
            }
            if(utf8::is_valid((const char8_t*)sPtr))
            {
                m_data = (const char8_t*)sPtr;
            }
            else
            {
                NAU_FAILURE(u8"Invalid input string.");
                m_data.clear();
            }
        }

        template <class S>
            requires std::is_constructible_v<eastl::wstring_view, S>
        string(S&& s)
        {
            auto sView = eastl::wstring_view(s);
            if(!sView.data())
            {
                m_data = u8"";
                return;
            }
            utf8::utf16to32(sView.begin(), sView.end(), std::back_inserter(m_data));

            if(!utf8::is_valid(m_data.c_str()))
            {
                NAU_FAILURE(u8"Invalid input string.");
                m_data.clear();
            }
        }

        template <class S>
            requires(!std::is_constructible_v<eastl::wstring_view, S>) &&
                    std::is_constructible_v<std::wstring_view, S>
        string(S&& s)
        {
            auto sView = std::wstring_view(s);
            utf8::utf16to32(sView.begin(), sView.end(), std::back_inserter(m_data));  // TODO locale
        }

#pragma endregion
#pragma region string_constructors

        template <class S>
            requires std::is_constructible_v<eastl::u8string_view, S>
        string(S&& s) :
            m_data(eastl::u8string_view(s).data())
        {
        }

        template <class S>
            requires std::is_constructible_v<eastl::u16string_view, S>
        string(S&& s)
        {
            auto sView = eastl::u16string_view(s);
            utf8::utf16to32(sView.begin(), sView.end(), std::back_inserter(m_data));
        }

        template <class S>
            requires std::is_constructible_v<eastl::u32string_view, S>
        string(S&& s)
        {
            auto sView = eastl::u32string_view(s);
            for(size_t i = 0; i < sView.size(); i++)
            {
                m_data.push_back(sView[i]);
            }
        }

        template <class S>
            requires(!std::is_constructible_v<eastl::u8string_view, S>) &&
                    std::is_constructible_v<std::u8string_view, S>
        string(S&& s) :
            string(std::u8string_view(s).data())
        {
        }

        template <class S>
            requires(!std::is_constructible_v<eastl::u16string_view, S>) &&
                    std::is_constructible_v<std::u16string_view, S>
        string(S&& s) :
            string(std::u16string_view(s).data())
        {
        }

        template <class S>
            requires(!std::is_constructible_v<eastl::u32string_view, S>) &&
                    std::is_constructible_v<std::u32string_view, S>
        string(S&& s) :
            string(std::u32string_view(s).data())
        {
        }

        string(string_view strView);

#pragma endregion

#pragma region conversions
        eastl::string tostring() const
        {
            return eastl::string((const char*)m_data.c_str());
        }

        eastl::u8string tou8string() const
        {
            return eastl::u8string(m_data.c_str());
        }

        eastl::u16string tou16string() const
        {
            eastl::u16string output;
            utf8::utf8to16(m_data.raw_begin(), m_data.raw_end(), std::back_inserter(output));
            return output;
        }

        eastl::u32string tou32string() const
        {
            eastl::u32string output;
            utf8::utf8to32(m_data.raw_begin(), m_data.raw_end(), std::back_inserter(output));
            return output;
        }
#pragma endregion

#pragma region operators
        inline reference operator[](size_t n) noexcept
        {
            return m_data[n];
        }

        inline value_type operator[](size_t n) const noexcept
        {
            return m_data[n];
        }

        inline value_type at(size_type n) const
        {
            return m_data.at(n);
        }
        inline value_type at(size_type n, std::nothrow_t) const noexcept
        {
            return m_data.at(n);
        }

        inline char8_t* raw_at(size_t n) noexcept
        {
            return m_data.data() + m_data.get_num_bytes_from_start(n);
        }

        inline const char8_t* raw_at(size_t n) const noexcept
        {
            return m_data.data() + m_data.get_num_bytes_from_start(n);
        }

        inline string substr(size_t pos, size_t n = npos) noexcept
        {
            return string{m_data.substr(pos, n)};
        }
        //! Equality Comparison Operators
        inline std::strong_ordering operator<=>(const string& str) const noexcept
        {
            return {(signed char)m_data.compare(str.m_data)};
        }

        inline bool operator==(const string&) const = default;
        inline bool operator!=(const string&) const = default;
        inline bool operator<(const string&) const = default;
        inline bool operator>(const string&) const = default;
        inline bool operator<=(const string&) const = default;
        inline bool operator>=(const string&) const = default;

        template <class S>
            requires(std::is_constructible_v<string, S>) && (!std::is_same_v<string, std::remove_cvref_t<S>>)
        inline bool operator==(S&& rhs) const
        {
            return *this == string(rhs);
        };
        template <class S>
            requires(std::is_constructible_v<string, S>) && (!std::is_same_v<string, std::remove_cvref_t<S>>)
        inline bool operator!=(S&& rhs) const
        {
            return *this != string(rhs);
        };
        template <class S>
            requires(std::is_constructible_v<string, S>) && (!std::is_same_v<string, std::remove_cvref_t<S>>)
        inline bool operator<(S&& rhs) const
        {
            return *this < string(rhs);
        };
        template <class S>
            requires(std::is_constructible_v<string, S>) && (!std::is_same_v<string, std::remove_cvref_t<S>>)
        inline bool operator>(S&& rhs) const
        {
            return *this > string(rhs);
        };
        template <class S>
            requires(std::is_constructible_v<string, S>) && (!std::is_same_v<string, std::remove_cvref_t<S>>)
        inline bool operator<=(S&& rhs) const
        {
            return *this <= string(rhs);
        };
        template <class S>
            requires(std::is_constructible_v<string, S>) && (!std::is_same_v<string, std::remove_cvref_t<S>>)
        inline bool operator>=(S&& rhs) const
        {
            return *this >= string(rhs);
        };
#pragma endregion

        inline const data_type* c_str() const noexcept
        {
            return m_data.c_str();
        }
        inline const data_type* data() const noexcept
        {
            return m_data.data();
        }
        inline data_type* data() noexcept
        {
            return m_data.data();
        }

        inline size_type length() const noexcept
        {
            return m_data.length();
        }

        inline size_type size() const noexcept
        {
            return m_data.size();
        }

        inline bool empty() const noexcept
        {
            return m_data.empty();
        }

        inline void clear()
        {
            m_data.clear();
        }

        inline void erase(size_type at, size_type count) noexcept
        {
          m_data.erase(at, count);
        }

        inline void erase(size_type at) noexcept
        {
          m_data.erase(at);
        }

        inline void insert(size_type at, const eastl::string& str)
        {
            m_data.insert(at, string{str}.m_data);
        }

        inline operator eastl::u8string_view() const
        {
            return {c_str(), size()};
        }

        inline operator std::u8string_view() const
        {
            return {c_str(), size()};
        }

#pragma region iterators

        inline iterator begin() noexcept
        {
            return m_data.begin();
        }
        inline const_iterator begin() const noexcept
        {
            return m_data.begin();
        }

        inline iterator end() noexcept
        {
            return m_data.end();
        }
        inline const_iterator end() const noexcept
        {
            return m_data.end();
        }
        inline reverse_iterator rbegin() noexcept
        {
            return m_data.rbegin();
        }
        inline const_reverse_iterator rbegin() const noexcept
        {
            return m_data.rbegin();
        }

        inline reverse_iterator rend() noexcept
        {
            return m_data.rend();
        }
        inline const_reverse_iterator rend() const noexcept
        {
            return m_data.rend();
        }
        inline const_iterator cbegin() const noexcept
        {
            return m_data.cbegin();
        }
        inline const_iterator cend() const noexcept
        {
            return m_data.cend();
        }
        inline const_reverse_iterator crbegin() const noexcept
        {
            return m_data.crbegin();
        }

        inline const_reverse_iterator crend() const noexcept
        {
            return m_data.crend();
        }
#pragma endregion

#pragma region append_functions
    private:
        inline string& prepend(const string& prependix) noexcept(TINY_UTF8_NOEXCEPT)
        {
            m_data.raw_insert(0, prependix.m_data);
            return *this;
        }

    public:
        string& append(const string& appendix) noexcept(TINY_UTF8_NOEXCEPT) 
        {
            m_data.append(appendix.m_data);
            return *this;
        };
        inline string& operator+=(const string& appendix) noexcept(TINY_UTF8_NOEXCEPT)
        {
            return append(appendix);
        }

        inline string& push_back(value_type cp) noexcept(TINY_UTF8_NOEXCEPT)
        {
            return append(string(cp));
        }
        inline string& operator+=(value_type cp) noexcept(TINY_UTF8_NOEXCEPT)
        {
            return append(string(cp));
        }

        inline string operator+(string summand) const& noexcept(TINY_UTF8_NOEXCEPT)
        {
            summand.prepend(*this);
            return summand;
        }
        inline string operator+(const string& summand) && noexcept(TINY_UTF8_NOEXCEPT)
        {
            append(summand);
            return static_cast<string&&>(*this);
        }
        friend inline string operator+(string lhs, data_type rhs) noexcept(TINY_UTF8_NOEXCEPT)
        {
            lhs.push_back(rhs);
            return lhs;
        }
        friend inline string operator+(string lhs, value_type rhs) noexcept(TINY_UTF8_NOEXCEPT)
        {
            lhs.push_back(rhs);
            return lhs;
        }
#pragma endregion

#pragma region format_functions
    public:
        template <class S, typename... Args>
            requires std::is_constructible_v<nau::string, S>
        static inline string format(S&& formatString, Args&&... args)
        {
            return nau::utils::format(nau::string(formatString).c_str(), std::forward<Args>(args)...);
        }

        template <class S, typename... Args>
            requires std::is_same_v<std::remove_cvref_t<S>, string>
        static inline string format_to(string& out, S&& formatString, Args&&... args)
        {
            out = nau::utils::format(nau::string(formatString).c_str(), std::forward<Args>(args)...);
            return out;
        }

        template <class S, typename = eastl::enable_if_t<std::is_constructible_v<eastl::u8string_view, S>>, typename... Args>
        static inline void format_to(string& out, S&& formatString, Args&&... args)
        {
            out = nau::utils::format(nau::string(formatString).c_str(), std::forward<Args>(args)...);
        }

        
        template <class S, typename = eastl::enable_if_t<std::is_constructible_v<string, S>>, typename... Args>
            requires (sizeof...(Args)>0)
        string(S&& s, Args&&... args)
        {
            *this = string::format(nau::string{s}, std::forward<Args>(args)...);
        }

        template <typename... Args>
        inline string format(Args&&... args)
        {
            return string::format(*this, std::forward<Args>(args)...);
        }

        template <typename... Args>
        inline void format_inline(Args&&... args)
        {
            *this = string::format(*this, std::forward<Args>(args)...);
        }

        template <typename S, typename... Args>
            requires std::is_constructible_v<nau::string, S>
        inline void append_format(S&& formatString, Args&&... args)
        {
            auto fomatrRes = string::format(formatString, std::forward<Args>(args)...);
            this->append(fomatrRes);
        }

        template <>
        inline string format()
        {
            return *this;
        }

        template <>
        inline void format_inline()
        {
            return;
        }
#pragma endregion

#pragma region find_functions

        /**
         * Finds a specific codepoint inside the basic_string starting at the supplied codepoint index
         *
         * @param	cp				The codepoint to look for
         * @param	start_codepoint	The index of the first codepoint to start looking from
         * @return	The codepoint index where and if the codepoint was found or basic_string::npos
         */
        size_type find(value_type cp, size_type start_codepoint = 0) const noexcept
        {
            return m_data.find(cp, start_codepoint);
        }
        /**
         * Finds a specific pattern within the basic_string starting at the supplied codepoint index
         *
         * @param	cp				The codepoint to look for
         * @param	start_codepoint	The index of the first codepoint to start looking from
         * @return	The codepoint index where and if the pattern was found or basic_string::npos
         */
        size_type find(const string& pattern, size_type start_codepoint = 0) const noexcept
        {
            return m_data.find(pattern.m_data, start_codepoint);
        }
        /**
         * Finds a specific pattern within the basic_string starting at the supplied codepoint index
         *
         * @param	cp				The codepoint to look for
         * @param	start_codepoint	The index of the first codepoint to start looking from
         * @return	The codepoint index where and if the pattern was found or basic_string::npos
         */
        size_type find(const data_type* pattern, size_type start_codepoint = 0) const noexcept
        {
            return m_data.find(pattern, start_codepoint);
        }

#pragma endregion
    };

    class string_view
    {
    public:
        using this_type = string_view;
        using value_type = string::value_type;
        using reference = string::reference;
        using const_reference = string::const_reference;
        using iterator = string::iterator;
        using const_iterator = string::const_iterator;
        using reverse_iterator = string::reverse_iterator;
        using const_reverse_iterator = string::const_reverse_iterator;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        static const constexpr size_type npos = size_type(-1);

    protected:
        const string* m_data = nullptr;
        size_type m_shift = 0;
        size_type m_count = 0;

    public:
        constexpr string_view() noexcept :
            m_data(nullptr),
            m_shift(),
            m_count(0)
        {
        }
        string_view(const string_view& other) noexcept = default;
        string_view(string_view&& other) noexcept
        {
            m_data = other.m_data;
            m_shift = other.m_shift;
            m_count = other.m_count;
            other.m_data = nullptr;
            other.m_count = 0;
            other.m_shift = 0;
        };

        string_view& operator=(string_view&& other) noexcept
        {
            m_data = other.m_data;
            m_shift = other.m_shift;
            m_count = other.m_count;
            other.m_data = nullptr;
            other.m_shift = 0;
            other.m_count = 0;
            return *this;
        };

        string_view& operator=(const string_view& other) = default;

        string_view(const string* s, size_type count) :
            m_data(s),
            m_count(count)
        {
        }
        string_view(const string* s, size_type count, size_type shift) :
            m_data(s),
            m_shift(shift),
            m_count(count)
        {
        }
        string_view(const string* s) :
            m_data(s),
            m_shift(0),
            m_count(s != nullptr ? s->size() : 0)
        {
        }

        string_view(const string& s) :
            m_data(&s),
            m_shift(0),
            m_count(s.length())
        {
        }

        string_view(string_view str, size_type count, size_type shift) :
            m_data(str.m_data),
            m_shift(str.m_shift + shift),
            m_count(count)
        {
            NAU_ASSERT(m_data != nullptr || m_count == 0);
            NAU_ASSERT(m_data == nullptr || (m_shift + m_count <= m_data->size()));
        }

        string_view(string&& s) = delete;

        const_iterator begin() const noexcept
        {
            return m_data->begin() + m_shift;
        }
        const_iterator cbegin() const noexcept
        {
            return m_data->cbegin() + m_shift;
        }
        const_iterator end() const noexcept
        {
            return m_data->begin() + m_shift + m_count;
        }
        const_iterator cend() const noexcept
        {
            return m_data->cbegin() + m_shift + m_count;
        }
        const_reverse_iterator rbegin() const noexcept
        {
            return m_data->rbegin() + (m_data->length() - (m_shift + m_count));
        }
        const_reverse_iterator crbegin() const noexcept
        {
            return m_data->crbegin() + (m_data->length() - (m_shift + m_count));
        }
        const_reverse_iterator rend() const noexcept
        {
            return m_data->rend() - m_shift;
        }
        const_reverse_iterator crend() const noexcept
        {
            return m_data->crend() - m_shift;
        }

        const char8_t* data() const
        {
            return m_data->raw_at(m_shift);
        }

        const char8_t* c_str() const
        {
            return m_data->raw_at(m_shift);
        }

        value_type front() const
        {
            NAU_ASSERT(!empty(), u8"Behavior is undefined if string_view is empty.");
            return (*m_data)[m_shift];
        }

        value_type back() const
        {
            NAU_ASSERT(!empty(), u8"Behavior is undefined if string_view is empty.");
            return (*m_data)[m_count - 1];
        }

        value_type operator[](size_type pos) const
        {
            // As per the standard spec: No bounds checking is performed: the behavior is undefined if pos >= size().
            return (*m_data)[m_shift + pos];
        }

        value_type at(size_type pos) const
        {
            NAU_ASSERT(pos < m_count, u8"nau_string_view::at {} out of range.", pos);
            return (*m_data)[m_shift + pos];
        }

        const char8_t* raw_at(size_type pos) const
        {
            NAU_ASSERT(pos < m_count, u8"nau_string_view::at {} out of range.", pos);
            return m_data->raw_at(m_shift + pos);
        }

        size_type size() const noexcept
        {
            return m_data->raw_at(m_shift + m_count) - data();
        }
        size_type length() const noexcept
        {
            return m_count;
        }

        size_type max_size() const noexcept
        {
            return (std::numeric_limits<size_type>::max)();
        }
        bool empty() const noexcept
        {
            return m_count == 0;
        }

        // 21.4.2.5, modifiers
        void swap(string_view& v)
        {
            eastl::swap(m_data, v.m_data);
            eastl::swap(m_count, v.m_count);
            eastl::swap(m_shift, v.m_shift);
        }

        void remove_prefix(size_type n)
        {
            NAU_ASSERT(n <= m_count, u8"behavior is undefined if moving past the end of the string");
            m_shift += n;
            m_count -= n;
        }

        void remove_suffix(size_type n)
        {
            NAU_ASSERT(n <= m_count, u8"behavior is undefined if moving past the beginning of the string");
            m_count -= n;
        }

        inline size_type copy(string& pDestination, size_type count, size_type pos = 0) const
        {
            NAU_ASSERT(pos < m_count, u8"nau_string_view::copy {} out of range.", pos);

            count = eastl::min<size_type>(count, m_count - pos);

            for(int i = 0; i < count; i++)
            {
                pDestination[i] = (*m_data)[m_shift + pos + i];
            }

            return count;
        }

        size_type copy(string* pDestination, size_type count, size_type pos = 0) const
        {
            return copy(*pDestination, count, pos);
        }

        string_view substr(size_type pos = 0, size_type count = npos) const
        {
            NAU_ASSERT(pos < m_count, u8"nau_string_view::substr {} out of range.", count);

            count = eastl::min<size_type>(count, m_count - pos);
            return string_view(m_data, count, m_shift + pos);
        }

        // static  int compare(const T* pBegin1, const T* pEnd1, const T* pBegin2, const T* pEnd2)
        //{
        //     const ptrdiff_t n1 = pEnd1 - pBegin1;
        //     const ptrdiff_t n2 = pEnd2 - pBegin2;
        //     const ptrdiff_t nMin = eastl::min_alt(n1, n2);
        //     const int cmp = Compare(pBegin1, pBegin2, (size_type)nMin);

        //    return (cmp != 0 ? cmp : (n1 < n2 ? -1 : (n1 > n2 ? 1 : 0)));
        //}

        //  int compare(nau_string_view sw) const noexcept
        //{
        //     return compare(mpBegin, mpBegin + mnCount, sw.mpBegin, sw.mpBegin + sw.mnCount);
        // }

        //  int compare(size_type pos1, size_type count1, nau_string_view sw) const
        //{
        //     return substr(pos1, count1).compare(sw);
        // }

        //  int compare(size_type pos1,
        //                       size_type count1,
        //                       nau_string_view sw,
        //                       size_type pos2,
        //                       size_type count2) const
        //{
        //     return substr(pos1, count1).compare(sw.substr(pos2, count2));
        // }

        //  int compare(const T* s) const
        //{
        //     return compare(nau_string_view(s));
        // }

        //  int compare(size_type pos1, size_type count1, const T* s) const
        //{
        //     return substr(pos1, count1).compare(nau_string_view(s));
        // }

        //  int compare(size_type pos1, size_type count1, const T* s, size_type count2) const
        //{
        //     return substr(pos1, count1).compare(nau_string_view(s, count2));
        // }

        // size_type find(nau_string_view sw, size_type pos = 0) const noexcept
        //{
        //     auto* pEnd = m_data + m_count;
        //     if(EASTL_LIKELY(((npos - sw.size()) >= pos) && (pos + sw.size()) <= m_count))
        //     {
        //         return m_data->find(sw.m_data->c_str(), pos);
        //     }
        //     return npos;
        // }

        //  size_type rfind(nau_string_view sw, size_type pos = npos) const noexcept
        //{
        //     return rfind(sw.mpBegin, pos, sw.mnCount);
        // }

        //  size_type rfind(T c, size_type pos = npos) const noexcept
        //{
        //    if(EASTL_LIKELY(mnCount))
        //    {
        //        const value_type* const pEnd = mpBegin + eastl::min_alt(mnCount - 1, pos) + 1;
        //        const value_type* const pResult = CharTypeStringRFind(pEnd, mpBegin, c);
        //
        //        if(pResult != mpBegin)
        //            return (size_type)((pResult - 1) - mpBegin);
        //    }
        //    return npos;
        //}

        //  size_type rfind(const T* s, size_type pos, size_type n) const
        //{
        //    // Disabled because it's not clear what values are valid for position.
        //    // It is documented that npos is a valid value, though. We return npos and
        //    // don't crash if postion is any invalid value.
        //    // #if EASTL_ASSERT_ENABLED
        //    //    if(EASTL_UNLIKELY((position != npos) && (position > (size_type)(mpEnd - mpBegin))))
        //    //        EASTL_FAIL_MSG("basic_string::rfind -- invalid position");
        //    // #endif
        //
        //    // Note that a search for a zero length string starting at position = end() returns end() and not npos.
        //    // Note by Paul Pedriana: I am not sure how this should behave in the case of n == 0 and position > size.
        //    // The standard seems to suggest that rfind doesn't act exactly the same as find in that input position
        //    // can be > size and the return value can still be other than npos. Thus, if n == 0 then you can
        //    // never return npos, unlike the case with find.
        //    if(EASTL_LIKELY(n <= mnCount))
        //    {
        //        if(EASTL_LIKELY(n))
        //        {
        //            const const_iterator pEnd = mpBegin + eastl::min_alt(mnCount - n, pos) + n;
        //            const const_iterator pResult = CharTypeStringRSearch(mpBegin, pEnd, s, s + n);
        //
        //            if(pResult != pEnd)
        //                return (size_type)(pResult - mpBegin);
        //        }
        //        else
        //            return eastl::min_alt(mnCount, pos);
        //    }
        //    return npos;
        //}

        //  size_type rfind(const T* s, size_type pos = npos) const
        //{
        //    return rfind(s, pos, (size_type)CharStrlen(s));
        //}

        //  size_type find_first_of(nau_string_view sw, size_type pos = 0) const noexcept
        //{
        //    return find_first_of(sw.mpBegin, pos, sw.mnCount);
        //}

        //  size_type find_first_of(T c, size_type pos = 0) const noexcept
        //{
        //    return find(c, pos);
        //}

        //  size_type find_first_of(const T* s, size_type pos, size_type n) const
        //{
        //    // If position is >= size, we return npos.
        //    if(EASTL_LIKELY((pos < mnCount)))
        //    {
        //        const value_type* const pBegin = mpBegin + pos;
        //        const value_type* const pEnd = mpBegin + mnCount;
        //        const const_iterator pResult = CharTypeStringFindFirstOf(pBegin, pEnd, s, s + n);
        //
        //        if(pResult != pEnd)
        //            return (size_type)(pResult - mpBegin);
        //    }
        //    return npos;
        //}

        //  size_type find_first_of(const T* s, size_type pos = 0) const
        //{
        //    return find_first_of(s, pos, (size_type)CharStrlen(s));
        //}

        //  size_type find_last_of(nau_string_view sw, size_type pos = npos) const noexcept
        //{
        //    return find_last_of(sw.mpBegin, pos, sw.mnCount);
        //}

        //  size_type find_last_of(T c, size_type pos = npos) const noexcept
        //{
        //    return rfind(c, pos);
        //}

        //  size_type find_last_of(const T* s, size_type pos, size_type n) const
        //{
        //    // If n is zero or position is >= size, we return npos.
        //    if(EASTL_LIKELY(mnCount))
        //    {
        //        const value_type* const pEnd = mpBegin + eastl::min_alt(mnCount - 1, pos) + 1;
        //        const value_type* const pResult = CharTypeStringRFindFirstOf(pEnd, mpBegin, s, s + n);
        //
        //        if(pResult != mpBegin)
        //            return (size_type)((pResult - 1) - mpBegin);
        //    }
        //    return npos;
        //}

        //  size_type find_last_of(const T* s, size_type pos = npos) const
        //{
        //    return find_last_of(s, pos, (size_type)CharStrlen(s));
        //}

        //  size_type find_first_not_of(nau_string_view sw, size_type pos = 0) const noexcept
        //{
        //    return find_first_not_of(sw.mpBegin, pos, sw.mnCount);
        //}

        //  size_type find_first_not_of(T c, size_type pos = 0) const noexcept
        //{
        //    if(EASTL_LIKELY(pos <= mnCount))
        //    {
        //        const auto pEnd = mpBegin + mnCount;
        //        // Todo: Possibly make a specialized version of CharTypeStringFindFirstNotOf(pBegin, pEnd, c).
        //        const const_iterator pResult = CharTypeStringFindFirstNotOf(mpBegin + pos, pEnd, &c, &c + 1);
        //
        //        if(pResult != pEnd)
        //            return (size_type)(pResult - mpBegin);
        //    }
        //    return npos;
        //}

        //  size_type find_first_not_of(const T* s, size_type pos, size_type n) const
        //{
        //    if(EASTL_LIKELY(pos <= mnCount))
        //    {
        //        const auto pEnd = mpBegin + mnCount;
        //        const const_iterator pResult = CharTypeStringFindFirstNotOf(mpBegin + pos, pEnd, s, s + n);
        //
        //        if(pResult != pEnd)
        //            return (size_type)(pResult - mpBegin);
        //    }
        //    return npos;
        //}

        //  size_type find_first_not_of(const T* s, size_type pos = 0) const
        //{
        //    return find_first_not_of(s, pos, (size_type)CharStrlen(s));
        //}

        //  size_type find_last_not_of(nau_string_view sw, size_type pos = npos) const noexcept
        //{
        //   return find_last_not_of(sw.mpBegin, pos, sw.mnCount);
        //}

        //  size_type find_last_not_of(T c, size_type pos = npos) const noexcept
        //{
        //    if(EASTL_LIKELY(mnCount))
        //    {
        //        // Todo: Possibly make a specialized version of CharTypeStringRFindFirstNotOf(pBegin, pEnd, c).
        //        const value_type* const pEnd = mpBegin + eastl::min_alt(mnCount - 1, pos) + 1;
        //        const value_type* const pResult = CharTypeStringRFindFirstNotOf(pEnd, mpBegin, &c, &c + 1);
        //
        //        if(pResult != mpBegin)
        //            return (size_type)((pResult - 1) - mpBegin);
        //    }
        //    return npos;
        //}

        //  size_type find_last_not_of(const T* s, size_type pos, size_type n) const
        //{
        //    if(EASTL_LIKELY(mnCount))
        //    {
        //        const value_type* const pEnd = mpBegin + eastl::min_alt(mnCount - 1, pos) + 1;
        //        const value_type* const pResult = CharTypeStringRFindFirstNotOf(pEnd, mpBegin, s, s + n);
        //
        //        if(pResult != mpBegin)
        //            return (size_type)((pResult - 1) - mpBegin);
        //    }
        //    return npos;
        //}

        //  size_type find_last_not_of(const T* s, size_type pos = npos) const
        //{
        //    return find_last_not_of(s, pos, (size_type)CharStrlen(s));
        //}

        // starts_with
        // bool starts_with(nau_string_view x) const noexcept
        //{
        //    return (size() >= x.size()) && (find(x, 0) == 0);
        //}

        // ends_with
        //  bool ends_with(nau_string_view x) const noexcept
        //{
        //    return (size() >= x.size()) && (compare(size() - x.size(), npos, x) == 0);
        //}

        //  bool ends_with(T x) const noexcept
        //{
        //     return ends_with(nau_string_view(&x, 1));
        // }

        //  bool ends_with(const T* s) const
        //{
        //     return ends_with(nau_string_view(s));
        // }
    };

    inline string::string(string_view strView) :
        m_data(strView.data(), strView.size())
    {
    }

    namespace string_literals
    {
        _NODISCARD NAU_KERNEL_EXPORT string operator"" _ns(const char8_t* _Str, size_t _Len);
        _NODISCARD NAU_KERNEL_EXPORT string operator"" _ns(const char16_t* _Str, size_t _Len);
        _NODISCARD NAU_KERNEL_EXPORT string operator"" _ns(const char32_t* _Str, size_t _Len);
    }  // namespace string_literals

    using nauchar = char8_t;
}  // namespace nau

template <>
struct fmt::formatter<nau::string, char8_t> : fmt::formatter<const char8_t*, char8_t>
{
    auto format(nau::string a, fmt::buffered_context<char8_t>& ctx) const
    {
        return formatter<const char8_t*, char8_t>::format(a.c_str(), ctx);
    }
};

template <>
struct eastl::hash<nau::string> : eastl::hash<const char8_t*>
{
    size_t operator()(const nau::string& s) const
    {
        using Base = eastl::hash<const char8_t*>;

        const auto& base = static_cast<const Base&>(*this);
        return base(s.c_str());
    }
};

template <>
struct std::hash<nau::string> : std::hash<std::basic_string_view<char8_t>>
{
    size_t operator()(const nau::string& s) const
    {
        using Base = std::hash<std::basic_string_view<char8_t>>;

        const auto& base = static_cast<const Base&>(*this);
        return base(s.c_str());
    }
};
