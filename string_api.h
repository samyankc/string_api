#ifndef STRINGAPI_H
#define STRINGAPI_H

#include <algorithm>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace StringAPI
{
    auto LoadFileContent( const char* FileName ) -> std::string
    {
        using it_type = std::istreambuf_iterator<char>;
        std::ifstream Fin( FileName );
        if( Fin ) return { it_type( Fin ), it_type( /*default_sentinel*/ ) };
        return {};
    }

    struct Write
    {
        auto To( std::string_view Location )
        {
            using it_type = std::ostreambuf_iterator<char>;
            std::ofstream Fout( Location.data(), std::ofstream::trunc );
            if( Fout ) std::copy( Source.begin(), Source.end(), it_type( Fout ) );
        }
        std::string_view Source;
    };

    struct Search
    {
        auto In( std::string_view Source )
        {
            return std::search( Source.begin(), Source.end(),  //
                                std::boyer_moore_searcher{ Pattern.begin(), Pattern.end() } );
        }
        std::string_view Pattern;
    };

    struct Trim
    {
        friend auto operator|( std::string_view Source, const Trim& Adaptor )
        {
            if( auto pos = Source.find_first_not_of( Adaptor.Exclude ); pos != Source.npos )
                Source.remove_prefix( pos );
            else
                Source.remove_prefix( Source.length() );

            if( auto pos = Source.find_last_not_of( Adaptor.Exclude ); pos != Source.npos )
                Source.remove_suffix( Source.length() - pos - 1 );

            return Source;
        }
        const char* Exclude;
    };

    struct After
    {
        friend auto operator|( std::string_view Source, const After& Adaptor )
        {
            auto Start = Search( Adaptor.LeftBound ).In( Source ) + Adaptor.LeftBound.length();
            auto End   = Source.end();
            return Start < End ? std::string_view{ Start, End } : std::string_view{ End, 0 };
        }
        friend void      operator|=( auto& Source, const After& Adaptor ) { Source = Source | Adaptor; }
        std::string_view LeftBound;
    };

    struct Between
    {
        friend auto operator|( std::string_view Source, const Between& Adaptor )
        {
            auto Start = Search( Adaptor.LeftBound ).In( Source ) + Adaptor.LeftBound.length();
            auto End   = Search( Adaptor.RightBound ).In( { Start, Source.end() } );
            return Start < End ? std::string_view{ Start, End } : std::string_view{ End, 0 };
        }
        friend void      operator|=( auto& Source, const Between& Adaptor ) { Source = Source | Adaptor; }
        std::string_view LeftBound, RightBound;
    };

    struct Count
    {
        auto In( std::string_view Source )
        {
            auto BMS      = std::boyer_moore_searcher{ Pattern.begin(), Pattern.end() };
            auto Count    = -1;
            auto NewFound = Source.begin();
            for( auto NewFound = Source.begin();  //
                 NewFound != Source.end();        //
                 NewFound = std::search( Source.begin(), Source.end(), BMS ) )
            {
                ++Count;
                Source = std::string_view{ NewFound + Pattern.length(), Source.end() };
            }
            return Count;
        }
        std::string_view Pattern;
    };


    struct Split_
    {
        std::string_view BaseRange;

        auto By( const char Delimiter )
        {
            auto RangeBegin = BaseRange.begin();
            auto RangeEnd   = BaseRange.end();

            auto Result = std::vector<std::string_view>{};
            Result.reserve( std::count( RangeBegin, RangeEnd, Delimiter ) + 1 );

            while( RangeBegin != RangeEnd )
            {
                auto DelimiterPos = std::find( RangeBegin, RangeEnd, Delimiter );
                Result.emplace_back( RangeBegin, DelimiterPos );
                if( DelimiterPos == RangeEnd )
                    RangeBegin = RangeEnd;
                else
                    RangeBegin = DelimiterPos + 1;
            }

            return Result;
        }
    };


    struct Split
    {
        std::string_view BaseRange;

        struct InternalItorSentinel
        {};

        struct InternalItor
        {
            std::string_view BaseRange;
            const char       Delimiter;

            auto operator*()
            {
                auto DelimiterPos = std::find( BaseRange.begin(), BaseRange.end(), Delimiter );
                return std::string_view{ BaseRange.begin(), DelimiterPos };
            }

            auto operator!=( InternalItorSentinel ) { return ! BaseRange.empty(); }
            auto operator++()
            {
                auto DelimiterPos = std::find( BaseRange.begin(), BaseRange.end(), Delimiter );
                if( DelimiterPos == BaseRange.end() )
                    BaseRange.remove_suffix( BaseRange.length() );
                else
                    BaseRange = std::string_view{ DelimiterPos + 1, BaseRange.end() };
            }
        };

        struct InternalRange
        {
            std::string_view BaseRange;
            const char       Delimiter;

            auto begin() { return InternalItor{ BaseRange, Delimiter }; }
            auto end() { return InternalItorSentinel{}; }
            auto size()
            {
                return ! BaseRange.ends_with( Delimiter )  // ending delim adjustment
                     + std::count( BaseRange.begin(), BaseRange.end(), Delimiter );
            }
        };

        auto By( const char Delimiter ) { return InternalRange{ BaseRange, Delimiter }; };
    };

    struct SplitBetween
    {
        std::string_view LeftDelimiter, RightDelimiter;

        struct InternalItorSentinel
        {};

        struct InternalItor
        {
            std::string_view BaseRange, LeftDelimiter, RightDelimiter;

            auto operator*() { return BaseRange | Between( "", RightDelimiter ); }
            auto operator!=( InternalItorSentinel ) { return ! BaseRange.empty(); }
            auto operator++()
            {
                BaseRange |= After( RightDelimiter );
                BaseRange |= After( LeftDelimiter );
                if( ! BaseRange.contains( RightDelimiter ) ) BaseRange.remove_prefix( BaseRange.length() );
            }
        };

        struct InternalRange
        {
            std::string_view BaseRange, LeftDelimiter, RightDelimiter;

            auto begin() { return InternalItor{ BaseRange | After( LeftDelimiter ), LeftDelimiter, RightDelimiter }; }
            auto end() { return InternalItorSentinel{}; }
        };

        friend auto operator|( std::string_view Source, const SplitBetween& Adaptor )
        {
            return InternalRange{ Source, Adaptor.LeftDelimiter, Adaptor.RightDelimiter };
        }
    };


    template<typename Predicate>
    struct DropIf
    {
        Predicate DropCondition;

        template<typename AncestorRange>
        struct InternalRange
        {
            AncestorRange BaseRange;
            Predicate     DropCondition;

            auto begin()
            {
                auto First = BaseRange.begin();
                while( ( First != BaseRange.end() ) && DropCondition( *First ) ) ++First;
                return InternalItor{ *this, First };
            }
            auto end() { return BaseRange.end(); }
        };

        template<typename AncestorRange, typename AncestorItor>
        struct InternalItor
        {
            AncestorRange EnclosingRange;
            AncestorItor  BaseItor;

            auto operator*() { return *BaseItor; }
            auto operator!=( const auto& RHS ) { return BaseItor != RHS; }
            auto operator++()
            {
                ++BaseItor;
                while( ( BaseItor != EnclosingRange.end() ) && EnclosingRange.DropCondition( *BaseItor ) ) ++BaseItor;
            }
        };

        friend auto operator|( auto SourceRange, const DropIf& Adaptor ) { return InternalRange{ SourceRange, Adaptor.DropCondition }; }
    };


    struct Take
    {
        int N;

        template<typename AncestorRange>
        struct InternalRange
        {
            AncestorRange BaseRange;
            int           N;
            auto          begin() { return InternalItor{ BaseRange.begin(), N }; }
            auto          end() { return BaseRange.end(); }
        };

        template<typename AncestorItor>
        struct InternalItor
        {
            AncestorItor BaseItor;
            int          N;

            auto operator*() { return *BaseItor; }
            auto operator!=( const auto& RHS ) { return N > 0 && BaseItor != RHS; }
            auto operator++()
            {
                if( N-- > 0 ) ++BaseItor;
            }
        };

        friend auto operator|( auto SourceRange, const Take& Adaptor ) { return InternalRange{ SourceRange, Adaptor.N }; }
    };


#define BatchReplace( ... ) \
    BatchReplace_impl { __VA_ARGS__ }

    using BatchReplaceSubstitutionPair = std::array<std::string_view, 2>;

    struct BatchReplace_impl : std::vector<BatchReplaceSubstitutionPair>
    {
        BatchReplace_impl( const std::initializer_list<BatchReplaceSubstitutionPair>& IL ) : std::vector<BatchReplaceSubstitutionPair>{ IL }
        {}

        auto In( std::string_view Source ) const
        {
            auto Result      = std::string{};
            auto TotalLength = Source.length();

            for( auto&& SubstitutionPair : *this )
                TotalLength
                += ( SubstitutionPair[ 1 ].length() - SubstitutionPair[ 0 ].length() ) * Count( SubstitutionPair[ 0 ] ).In( Source );

            Result.reserve( TotalLength );

            auto SourceBegin = Source.begin();
            auto SourceEnd   = Source.end();
            while( SourceBegin != SourceEnd )
            {
                auto TokenStart = std::find( SourceBegin, SourceEnd, '$' );

                while( TokenStart != SourceEnd &&  //
                       std::string_view{ TokenStart, TokenStart + 2 } != "${" )
                    TokenStart = std::find( TokenStart + 1, SourceEnd, '$' );

                Result.append( SourceBegin, TokenStart );

                if( TokenStart == SourceEnd ) return Result;

                auto TokenEnd = std::find( TokenStart + 2, SourceEnd, '}' );
                if( TokenEnd == SourceEnd )
                {
                    Result.append( TokenStart, SourceEnd );
                    return Result;
                }
                std::advance( TokenEnd, 1 );
                auto ReplaceToken      = std::string_view{ TokenStart, TokenEnd };
                auto ReplaceStringItor = std::find_if( begin(), end(),  //
                                                       [ = ]( const auto& E ) { return ReplaceToken == E[ 0 ]; } );
                if( ReplaceStringItor != end() )
                    Result.append( ( *ReplaceStringItor )[ 1 ] );
                else
                    Result.append( ReplaceToken );

                SourceBegin = TokenEnd;
            }

            return Result;
        }

        friend auto operator|( std::string_view Source, const BatchReplace_impl& Adaptor ) { return Adaptor.In( Source ); }
    };

    auto operator+( const std::string& LHS, const std::string_view& RHS ) -> std::string  //
    {
        return LHS + std::string{ RHS };
    }

    auto operator+( const std::string_view& LHS, const auto& RHS ) -> std::string  //
    {
        return std::string{ LHS } + RHS;
    }


    namespace TestSuite
    {
        auto TestBatchReplace()
        {
            return BatchReplace( { "${item1}", " item 1" },     //
                                 { "${a  b c}", "[ a b c ]" },  //
                                 { "${k}", "1234" },            //
                                 { "${unused}", "1231123" }, { "${last}", "!~LAST~!" }, )
            .In( "this is ${item1}, not ${item 1}; lets see ${a  b c}; its ${item1} again; ${k} can be replaced. and this is the ${last}" );
        }
    }  // namespace TestSuite

}  // namespace StringAPI

using StringAPI::operator+;

#endif
