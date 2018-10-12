#pragma once

#include <hs/hs.h>
#include <util/exception.h>
#include <vector>
#include <string>
#include <memory>

struct TRegexpDef {
    std::string Expr;
    unsigned int Flags = 0;

    TRegexpDef(std::string expr)
        : Expr(std::move(expr))
    {}

    TRegexpDef(std::string expr, unsigned int flags)
        : Expr(std::move(expr))
        , Flags(flags)
    {}
};

template <unsigned int MODE>
class TGenericRegexpDatabase {
public:
    TGenericRegexpDatabase(std::vector<TRegexpDef> regexps, unsigned int extra_mode = 0) {
        std::unique_ptr<const char*[]> strArray(new const char*[regexps.size()]);
        std::unique_ptr<unsigned int[]> regexpIds(new unsigned int[regexps.size()]);
        std::unique_ptr<unsigned int[]> regexpFlags(new unsigned int[regexps.size()]);

        for (size_t i = 0; i < regexps.size(); i++) {
            strArray[i] = regexps[i].Expr.c_str();
            regexpIds[i] = i;
            regexpFlags[i] = regexps[i].Flags;
        }

        hs_compile_error_t* error = nullptr;
        hs_error_t ret = hs_compile_multi(
            strArray.get(),
            regexpFlags.get(),
            regexpIds.get(),
            regexps.size(),
            MODE | extra_mode,
            nullptr,
            &Db_,
            &error
        );

        if (ret != HS_SUCCESS) {
            throw TException() << "cannot compile database: " << error->message;
        }
    }

    ~TGenericRegexpDatabase() {
        hs_free_database(Db_);
    }

    hs_database_t* Ptr() const {
        return Db_;
    }

private:
    hs_database_t* Db_ = nullptr;
};


using TBlockRegexpDatabase = TGenericRegexpDatabase<HS_MODE_BLOCK>;
using TVectoredRegexpDatabase = TGenericRegexpDatabase<HS_MODE_VECTORED>;
using TStreamRegexpDatabase = TGenericRegexpDatabase<HS_MODE_STREAM>;
