#ifndef SUBSTITUTE_H
#define SUBSTITUTE_H

#include "IRMutator.h"

namespace HalideInternal {

    /* Substitute an expression for a variable in a stmt or expr */
    Expr substitute(string name, Expr replacement, Expr expr);
    Stmt substitute(string name, Expr replacement, Stmt stmt);

    class Substitute : public IRMutator {
    public:
        Substitute(string var, Expr replacement);
    protected:
        string var;
        Expr replacement;
        void visit(const Var *v);
    };
};

#endif