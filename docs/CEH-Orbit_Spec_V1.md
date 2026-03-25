# CEH-Orbit Protocol Specification (Spec) V1.0

**Protocol Name**: CEH-Orbit Post-Quantum Authentication Protocol  
**Version**: V1.0  
**Author**: Chen Enhua  
**Identifier**: CEH_ORBIT_PROTOCOL_V1  
**Release Date**: March 2026  

---

## 1. Purpose

The CEH-Orbit protocol defines an authentication/signature mechanism based on lattice structures and orbit mapping.

Core idea:

- Generate high-dimensional orbit vectors from random masks  
- Compress orbit vectors into an `OrbitHead`  
- Derive challenge from `OrbitHead + Message`  
- Verify by reconstructing orbit and matching orbit head  

This specification aims to provide a **consistent, reproducible, and interoperable** implementation standard.

---

## 2. Terminology

| Term | Description |
|------|------------|
| BaseOrbitGen | Public base vector (A) |
| CoreVector_S | Private short vector (s) |
| PublicOrbit_T | Public vector T = A·s |
| StochasticMask | Random mask (y) |
| OrbitTrace_W | Orbit trace w = A·y |
| EncodedOrbit_Z | Response z = y + c·s |
| GeometricPivot | Challenge vector (c) |
| OrbitHead | Contains LSH + Phase |
| LSH | 128-bit locality-sensitive hash |
| Phase | K segments, values 0–3 |

---

## 3. Parameters

```c
#define LATTICE_DIM_N       128
#define MOD_Q               3329
#define ORBIT_ZONE_COUNT    16
#define ORBIT_WIDTH_CEH     32
#define RESPONSE_BOUND      260
#define PIVOT_WEIGHT        8
```

---

## 4. Mathematical Foundation

### Negacyclic Ring

R_q = Z_q[x] / (x^N + 1)

Thus:

x^N = -1

Polynomial multiplication:

c(x) = a(x) b(x) mod (x^N + 1)

This is implemented via **negacyclic convolution**.

---

## 5. Core Flow

### KeyGen

- Generate A ∈ R_q  
- Generate sparse s ∈ {-1,0,1}  
- Compute T = A·s  

---

### Sign

1. Sample y  
2. Compute w = A·y  
3. Compute H = BuildOrbitHead(w)  
4. Compute c = DeriveChallenge(H, msg)  
5. Compute z = y + c·s  
6. Reject if ||z|| too large  
7. Compute hash h  

---

### Verify

1. Check z bound  
2. Verify hash  
3. Compute w' = A·z − c·T  
4. Recompute H'  
5. Recompute c'  
6. Check consistency  

---

## 6. Security Notes

- Based on lattice hardness (heuristic)  
- No formal reduction yet  
- Research-level prototype  
- Not production-ready  

---

## 7. Final Definition

The protocol verifies:

DeriveChallenge(BuildOrbitHead(A·z − c·T), msg) == c

AND

BuildOrbitHead(A·z − c·T) == OrbitHead

Meaning:

> Verification checks **orbit consistency**, not exact algebraic equality.

---

## 8. License

© 2026 Chen Enhua  

Non-commercial use only.  
Commercial use requires written authorization.
