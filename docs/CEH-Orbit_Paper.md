# CEH-Orbit: An Orbit-Mapping-Based Post-Quantum Authentication Mechanism

**Chen Enhua**  
(Independent Researcher, China)  
Email: a106079595@qq.com  
Phone: +86 15557000007  

---

## Note

This is a **fully translated English version** of the original CEH-Orbit document.  
The structure, logic, and technical meaning are strictly preserved.

---

## Abstract

This paper proposes a novel post-quantum authentication mechanism, **CEH-Orbit**.  
Unlike traditional lattice-based signatures that rely on strict algebraic equality, CEH-Orbit introduces an intermediate geometric representation layer — **Orbit Mapping**.

This mapping compresses high-dimensional algebraic objects into low-dimensional discrete orbit descriptors, transforming the verification problem into an **orbit consistency check**.

We formally define the orbit mapping function F(w), introduce the concept of an acceptance basin, and establish an empirical framework for evaluating unforgeability. A complete engineering implementation is provided, including signing, verification, perturbation sensitivity analysis, and black-box attack simulations.

Experimental results demonstrate strong resistance against random forgery and local perturbation attacks under strict orbit-locking conditions.

**Keywords**: Post-quantum cryptography, lattice signatures, orbit mapping, Fiat-Shamir transform, orbit locking

---

## 1. Introduction

With the rapid advancement of quantum computing, traditional public-key cryptography is facing significant threats. Lattice-based cryptography (e.g., Dilithium, Falcon) has emerged as a leading candidate for post-quantum signatures.

These schemes rely on strict algebraic relations:

w = Ay  
z = y + cs  
w' = Az - ct  

Verification requires w' ≈ w within a tight bound.

Limitations:

- Low tolerance to perturbations  
- Purely algebraic interpretation  
- Attacks focus on breaking algebraic consistency  

We propose a new paradigm:

> Verification should check whether two states lie on the same orbit, rather than whether they are equal.

---

## 2. Preliminaries

Let:

R_q = Z_q[x] / (x^n + 1)

Where:

- n = 128  
- q = 3329  

We simplify the model by removing noise (t = As), making this a **research prototype**.

---

## 3. Orbit Mapping

### Definition

F(w) = (F_lsh(w), F_phase(w), optional F_stat(w))

---

### LSH Component

Let centered value:

w̄_i = center(w_i)

Define:

b_i = (floor(w̄_i / Δ) XOR i) mod 2

128 bits → two 64-bit integers.

---

### Phase Component

Split vector into K segments:

φ_k = (sum of segment k) mod 4

---

### Interpretation

Orbit mapping reduces high-dimensional algebra into discrete descriptors.  
Nearby inputs map to similar descriptors, while perturbations cause divergence.

---

## 4. Acceptance Basin

Distance:

d(w, w') = Hamming(LSH) + Phase distance

Strict mode:

τ = 0 → exact orbit match required

---

## 5. Protocol Construction

### Key Generation

- Sample A  
- Sample sparse s ∈ {-1,0,1}  
- Compute t = A·s  

---

### Signing

1. Sample y  
2. w = A·y  
3. H = F(w)  
4. c = SHA256(m || H)  
5. z = y + c·s  
6. Reject if ||z|| too large  
7. h = SHA256(z || c)  

---

### Verification

1. Check ||z||  
2. Check hash  
3. w' = A·z − c·t  
4. H' = F(w')  
5. c' = SHA256(m || H')  

Accept iff consistent.

---

## 6. Experiments

- Success rate: 100%  
- Forgery success: 0%  
- Perturbation success: 0%  
- Signature size: ~624 bytes  

---

## 7. Security Discussion

Advantages:

- Multi-layer verification  
- Nonlinear mapping  
- Binding hash  

Limitations:

- No formal reduction  
- Prototype-level parameters  
- No tolerance  

---

## 8. Future Work

- Formal proofs  
- Parameter scaling  
- Tolerant verification  
- NTT optimization  
- Hardware implementation  

---

## 9. Conclusion

CEH-Orbit introduces:

> Orbit-based verification instead of algebraic equality.

This establishes a new paradigm in post-quantum cryptography.

---

## References

Standard lattice cryptography and LSH literature.
