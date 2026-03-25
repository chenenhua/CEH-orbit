# CEH-Orbit Protocol Whitepaper (V1.0)

**Version**: 1.0  
**Date**: March 2025  
**Author**: Chen Enhua  
**Contact**: +86 15557000007 / a106079595@qq.com  

---

## 1. Abstract

This paper introduces a novel post-quantum authentication structure — the **CEH-Orbit Protocol (Orbit-based Post-Quantum Authentication Protocol)**.  
Built on lattice cryptography (Ring-LWE / R-SIS), the protocol departs from direct algebraic equality checks by introducing:

- Orbit Mapping  
- Orbit Locking  

High-dimensional algebraic objects are mapped into a low-dimensional discrete representation called the **Orbit Head**, which is then used for verification. The protocol provides triple protection via message binding, self-consistent challenge derivation, and orbit consistency, and demonstrates strong resistance to forgery under strict locking.

Main contributions:

- Definition of orbit mapping function F(w)  
- Introduction of acceptance basin and sensitivity function  
- Empirical unforgeability evaluation methodology  
- Full engineering implementation and attack-testing framework  
- Performance data and parameter rationale  

This work establishes the first engineering baseline for **Orbit Cryptography**.

---

## 2. Background and Motivation

Traditional lattice signatures (e.g., Dilithium, Falcon):

- Enforce strict algebraic equality  
- Provide strong reductions (MLWE / MSIS)  
- Allow no tolerance (exact matching required)

Limitations in practice:

- Poor tolerance to noise or physical deviations  
- Limited geometric interpretability  
- Insufficient sensitivity to micro-perturbations

We propose:

> Verify not whether points are equal, but whether they lie on the same orbit.

By mapping algebraic objects to geometric descriptors, we obtain a more intuitive and flexible authentication mechanism.

---

## 3. Protocol Overview

KeyGen → Sign → Verify

### 3.1 Key Generation

- Sample A ∈ R_q^{n×n}  
- Sample sparse s ∈ {-1,0,1}  
- Compute t = A·s  
- Output (A, t), s  

### 3.2 Signing

- Sample y ∈ [-200, 200]^n  
- Compute w = A·y  
- H = F(w)  
- c = SHA256(m || H) → sparse vector (weight κ=8)  
- z = y + c·s  
- Reject if ||z||∞ > 260  
- h = SHA256(z || c)  
- Output σ = (z, H, c, h)

### 3.3 Verification

- Check ||z||∞ ≤ 260  
- Check hash h  
- Compute w' = A·z − c·t  
- H' = F(w')  
- c' = SHA256(m || H')  
- Accept iff c'=c and H'=H

---

## 4. Mathematical Definition

R_q = Z_q[x]/(x^n + 1), with n=128, q=3329

Orbit mapping:

F(w) = (LSH(w), Phase(w))

LSH:
b_i = (floor(w_i/Δ) ⊕ i) mod 2

Phase:
Split into K segments:
φ_k = (sum segment) mod 4

---

## 5. Acceptance Basin

Distance:

d(w,w') = Hamming(LSH) + Phase distance

Strict mode:

τ = 0 → exact orbit match required

---

## 6. Unforgeability (Empirical)

- Random attack success ≈ 0  
- Local perturbation success ≈ 0  

Indicates strong empirical resistance.

---

## 7. Security Mechanisms

- Message binding  
- Domain separation  
- Rejection sampling  
- Binding hash  
- Orbit locking  

---

## 8. Experimental Results

Environment:

- Apple M1, 16GB  
- C++20, OpenSSL  

Results:

- Success rate: 100%  
- Forgery: 0%  
- Sign: 0.35 ms  
- Verify: 0.28 ms  
- Signature: ~624 bytes  

---

## 9. Design Philosophy

- Geometry-first verification  
- Closed-loop consistency  
- Experiment-driven security  

---

## 10. Limitations

- No formal reduction  
- No EUF-CMA proof  
- Parameters not standardized  
- No NTT optimization  
- No tolerance mode  

---

## 11. Future Work

- Formal reduction  
- Parameter scaling (n=256/512)  
- Tolerant orbit models  
- NTT acceleration  
- Hardware deployment  
- Standardization  

---

## 12. Conclusion

CEH-Orbit introduces a new paradigm:

> Orbit-based verification instead of algebraic equality.

This is a starting point for orbit cryptography.

---

## 13. References

Dilithium, Falcon, LSH, lattice cryptography literature

---

## License

CC BY-NC 4.0  
Commercial use requires authorization.
