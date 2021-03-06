#include "ModulusRemainder.h"
#include "IROperator.h"
#include "IRPrinter.h"
#include "IR.h"

// This file is largely a port of parts of src/analysis.ml
namespace Halide { 
namespace Internal {

class ComputeModulusRemainder : public IRVisitor {
public:
    ModulusRemainder analyze(Expr e);

    int modulus, remainder;
    Scope<ModulusRemainder> scope;

    using IRVisitor::visit;

    void visit(const IntImm *);
    void visit(const FloatImm *);
    void visit(const Cast *);
    void visit(const Variable *);
    void visit(const Add *);
    void visit(const Sub *);
    void visit(const Mul *);
    void visit(const Div *);
    void visit(const Mod *);
    void visit(const Min *);
    void visit(const Max *);
    void visit(const EQ *);
    void visit(const NE *);
    void visit(const LT *);
    void visit(const LE *);
    void visit(const GT *);
    void visit(const GE *);
    void visit(const And *);
    void visit(const Or *);
    void visit(const Not *);
    void visit(const Select *);
    void visit(const Load *);
    void visit(const Ramp *);
    void visit(const Broadcast *);
    void visit(const Call *);
    void visit(const Let *);
    void visit(const LetStmt *);
    void visit(const PrintStmt *);
    void visit(const AssertStmt *);
    void visit(const Pipeline *);
    void visit(const For *);
    void visit(const Store *);
    void visit(const Provide *);
    void visit(const Allocate *);
    void visit(const Realize *);
    void visit(const Block *);        
};

ModulusRemainder modulus_remainder(Expr e) {
    ComputeModulusRemainder mr;
    return mr.analyze(e);        
}

ModulusRemainder modulus_remainder(Expr e, const Scope<ModulusRemainder> &scope) {
    ComputeModulusRemainder mr;
    mr.scope = scope;
    return mr.analyze(e);        
}



bool reduce_expr_modulo(Expr expr, int modulus, int *remainder) {
    ModulusRemainder result = modulus_remainder(expr);

    /* As an example: If we asked for expr mod 8, and the analysis
     * said that expr = 16*k + 13, then because 16 % 8 == 0, the
     * result is 13 % 8 == 5. But if the analysis says that expr =
     * 6*k + 3, then expr mod 8 could be 1, 3, 5, or 7, so we just
     * return false. 
     */

    if (result.modulus % modulus == 0) {
        *remainder = result.remainder % modulus;
        return true;
    } else {
        return false;            
    }
}    

ModulusRemainder ComputeModulusRemainder::analyze(Expr e) {
    e.accept(this);
    return ModulusRemainder(modulus, remainder);
}

namespace {
void check(Expr e, int m, int r) {
    ModulusRemainder result = modulus_remainder(e);
    if (result.modulus != m || result.remainder != r) {
        std::cerr << "Test failed for modulus_remainder:\n";
        std::cerr << "Expression: " << e << "\n";
        std::cerr << "Correct modulus, remainder  = " << m << ", " << r << "\n";
        std::cerr << "Computed modulus, remainder = " 
                  << result.modulus << ", " 
                  << result.remainder << "\n";
        exit(-1);
    }
}
}

void modulus_remainder_test() {
    Expr x = Variable::make(Int(32), "x");
    Expr y = Variable::make(Int(32), "y");
    
    check((30*x + 3) + (40*y + 2), 10, 5);   
    check((6*x + 3) * (4*y + 1), 2, 1); 
    check(max(30*x - 24, 40*y + 31), 5, 1);
    check(10*x - 33*y, 1, 0);
    check(10*x - 35*y, 5, 0);
    check(123, 0, 123);
    check(Let::make("y", x*3 + 4, y*3 + 4), 9, 7);

    std::cerr << "modulus_remainder test passed\n";
}


void ComputeModulusRemainder::visit(const IntImm *op) {
    // Equal to op->value modulo anything. We'll use zero as the
    // modulus to mark this special case. We'd better be able to
    // handle zero in the rest of the code...
    remainder = op->value;
    modulus = 0;
}

void ComputeModulusRemainder::visit(const FloatImm *) {
    assert(false && "modulus_remainder of float");
}

void ComputeModulusRemainder::visit(const Cast *) {
    modulus = 1;
    remainder = 0;
}

void ComputeModulusRemainder::visit(const Variable *op) {
    if (scope.contains(op->name)) {
        ModulusRemainder mod_rem = scope.get(op->name);
        modulus = mod_rem.modulus;
        remainder = mod_rem.remainder;
    } else {
        modulus = 1;
        remainder = 0;
    }
}

int gcd(int a, int b) {
    if (a < b) std::swap(a, b);
    while (b != 0) {
        int tmp = b;
        b = a % b;
        a = tmp;            
    }
    return a;
}

int mod(int a, int m) {
    if (m == 0) return a;
    a = a % m;
    if (a < 0) a += m;
    return a;
}

void ComputeModulusRemainder::visit(const Add *op) {
    ModulusRemainder a = analyze(op->a);
    ModulusRemainder b = analyze(op->b);
    modulus = gcd(a.modulus, b.modulus);
    remainder = mod(a.remainder + b.remainder, modulus);
}

void ComputeModulusRemainder::visit(const Sub *op) {
    ModulusRemainder a = analyze(op->a);
    ModulusRemainder b = analyze(op->b);
    modulus = gcd(a.modulus, b.modulus);
    remainder = mod(a.remainder - b.remainder, modulus);
}

void ComputeModulusRemainder::visit(const Mul *op) {
    ModulusRemainder a = analyze(op->a);
    ModulusRemainder b = analyze(op->b);
    
    if (a.modulus == 0) {
        // a is constant
        modulus = a.remainder * b.modulus;
        remainder = a.remainder * b.remainder;
    } else if (b.modulus == 0) {
        // b is constant
        modulus = b.remainder * a.modulus;
        remainder = a.remainder * b.remainder;
    } else if (a.remainder == 0 && b.remainder == 0) {
        // multiple times multiple
        modulus = a.modulus * b.modulus;
        remainder = 0;
    } else if (a.remainder == 0) {
        modulus = a.modulus * gcd(b.modulus, b.remainder);
        remainder = 0;
    } else if (b.remainder == 0) {
        modulus = b.modulus * gcd(a.modulus, a.remainder);
        remainder = 0;
    } else {
        // All our tricks failed. Convert them to the same modulus and multiply
        modulus = gcd(a.modulus, b.modulus);
        a.remainder = mod(a.remainder * b.remainder, modulus);        
    }
}

void ComputeModulusRemainder::visit(const Div *) {
    // We might be able to say something about this if the numerator
    // modulus is provably a multiple of a constant denominator, but
    // in this case we should have simplified away the division.
    remainder = 0;
    modulus = 1;
}

namespace {
ModulusRemainder unify_alternatives(ModulusRemainder a, ModulusRemainder b) {
    // We don't know if we're going to get a or b, so we'd better find
    // a single modulus remainder that works for both. 

    // For example:
    // max(30*_ + 13, 40*_ + 27) ->
    // max(10*_ + 3, 10*_ + 7) ->
    // max(2*_ + 1, 2*_ + 1) ->
    // 2*_ + 1

    // Reduce them to the same modulus and the same remainder       
    int modulus = gcd(a.modulus, b.modulus);
    int diff = a.remainder - b.remainder;
    if (diff < 0) diff = -diff;
    modulus = gcd(diff, modulus);
    int ra = mod(a.remainder, modulus);
    int rb = mod(b.remainder, modulus);

    assert(ra == rb && "There's a bug inside ModulusRemainder in unify_alternatives");

    return ModulusRemainder(modulus, ra);
}
}

void ComputeModulusRemainder::visit(const Mod *op) {   
    // We can treat x mod y as x + z*y, where we know nothing about z.
    // (ax + b) + z (cx + d) ->
    // ax + b + zcx + dz ->
    // gcd(a, c, d) * w + b

    // E.g:
    // (8x + 5) mod (6x + 2) ->
    // (8x + 5) + z (6x + 2) ->
    // (8x + 6zx + 2x) + 5 ->
    // 2(4x + 3zx + x) + 5 ->
    // 2w + 1
    ModulusRemainder a = analyze(op->a);
    ModulusRemainder b = analyze(op->b);    
    modulus = gcd(a.modulus, b.modulus);
    modulus = gcd(modulus, b.remainder);
    remainder = mod(a.remainder, modulus);
} 

void ComputeModulusRemainder::visit(const Min *op) {
    ModulusRemainder r = unify_alternatives(analyze(op->a), analyze(op->b));
    modulus = r.modulus;
    remainder = r.remainder;
}

void ComputeModulusRemainder::visit(const Max *op) {
    ModulusRemainder r = unify_alternatives(analyze(op->a), analyze(op->b));
    modulus = r.modulus;
    remainder = r.remainder;
}

void ComputeModulusRemainder::visit(const EQ *) {
    assert(false && "modulus_remainder of bool");
}

void ComputeModulusRemainder::visit(const NE *) {
    assert(false && "modulus_remainder of bool");
}

void ComputeModulusRemainder::visit(const LT *) {
    assert(false && "modulus_remainder of bool");
}

void ComputeModulusRemainder::visit(const LE *) {
    assert(false && "modulus_remainder of bool");
}

void ComputeModulusRemainder::visit(const GT *) {
    assert(false && "modulus_remainder of bool");
}

void ComputeModulusRemainder::visit(const GE *) {
    assert(false && "modulus_remainder of bool");
}

void ComputeModulusRemainder::visit(const And *) {
    assert(false && "modulus_remainder of bool");
}

void ComputeModulusRemainder::visit(const Or *) {
    assert(false && "modulus_remainder of bool");
}

void ComputeModulusRemainder::visit(const Not *) {
    assert(false && "modulus_remainder of bool");
}

void ComputeModulusRemainder::visit(const Select *op) {
    ModulusRemainder r = unify_alternatives(analyze(op->true_value), 
                                            analyze(op->false_value));
    modulus = r.modulus;
    remainder = r.remainder;
}

void ComputeModulusRemainder::visit(const Load *) {
    modulus = 1;
    remainder = 0;
}

void ComputeModulusRemainder::visit(const Ramp *) {
    assert(false && "modulus_remainder of vector");
}

void ComputeModulusRemainder::visit(const Broadcast *) {
    assert(false && "modulus_remainder of vector");
}

void ComputeModulusRemainder::visit(const Call *) {
    modulus = 1;
    remainder = 0;
}

void ComputeModulusRemainder::visit(const Let *op) {
    ModulusRemainder val = analyze(op->value);
    scope.push(op->name, val);
    val = analyze(op->body);
    scope.pop(op->name);

    modulus = val.modulus;
    remainder = val.remainder;
}

void ComputeModulusRemainder::visit(const LetStmt *) {
    assert(false && "modulus_remainder of statement");
}

void ComputeModulusRemainder::visit(const PrintStmt *) {
    assert(false && "modulus_remainder of statement");
}

void ComputeModulusRemainder::visit(const AssertStmt *) {
    assert(false && "modulus_remainder of statement");
}

void ComputeModulusRemainder::visit(const Pipeline *) {
    assert(false && "modulus_remainder of statement");
}

void ComputeModulusRemainder::visit(const For *) {
    assert(false && "modulus_remainder of statement");
}

void ComputeModulusRemainder::visit(const Store *) {
    assert(false && "modulus_remainder of statement");
}

void ComputeModulusRemainder::visit(const Provide *) {
    assert(false && "modulus_remainder of statement");
}

void ComputeModulusRemainder::visit(const Allocate *) {
    assert(false && "modulus_remainder of statement");
}

void ComputeModulusRemainder::visit(const Realize *) {
    assert(false && "modulus_remainder of statement");
}

void ComputeModulusRemainder::visit(const Block *) {
    assert(false && "modulus_remainder of statement");
}

}
}
