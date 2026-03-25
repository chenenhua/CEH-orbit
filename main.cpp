/**
 * @file CEH_Orbit
 * @author Chen Enhua (陈恩华)
 * @brief 单文件公开调试版：CEH-Orbit PQC
 *
 * 说明：
 * 1. 本文件是“单文件完整版本”，协议逻辑 + UI 全部放在一个 .cpp 里
 * 2. 方便你直接丢进 Qt / CLion / CMake 项目里编译调试
 * 3. 保持黑色工业风 UI，左侧控制 + 日志，右侧实时渲染
 * 4. 数据全部直接渲染：
 *    - LSH 128bit 网格
 *    - Phase 柱状图
 *    - EncodedOrbit_Z 波形
 *    - RecoveredOrbit_W 波形
 *    - 验证结果 / 攻击结果 / 参数状态
 *
 * 编译依赖：
 * - Qt6 Widgets
 * - OpenSSL Crypto
 *
 * 若你是 qmake / CMake 项目，只要把本文件加入工程并链接 Qt Widgets + OpenSSL 即可。
 */

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QSplitter>
#include <QScrollArea>
#include <QFontMetrics>
#include <QSizePolicy>
#include <QString>
#include <QDateTime>
#include <QPainterPath>
#include <QCheckBox>

#include <openssl/sha.h>

#include <vector>
#include <array>
#include <string>
#include <random>
#include <set>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <ctime>

using namespace std;

// ============================================================
// 一、协议元数据
// ============================================================
namespace CEH_Orbit_V1 {

static const char* ARCH_IDENTIFIER =
    "CEH-ORBIT-PQC-V13-NONLINEAR-LATTICE-BY-CHEN-ENHUA";
static const char* COPYRIGHT_NOTICE =
    "Copyright (c) 2026 Chen Enhua. All Rights Reserved.";
static const char* PROTOCOL_TITLE =
    "陈恩华轨道抗量子签名体系 (CEH-Orbit PQC V1 单文件调试版)";

static const string CEH_DOMAIN_HEAD      = "CEH_ORBIT_HEAD_V1";
static const string CEH_DOMAIN_CHALLENGE = "CEH_ORBIT_CHALLENGE_V1";
static const string CEH_DOMAIN_BIND      = "CEH_ORBIT_BIND_V1";

static const int CEH_MAGIC = 0x434548;

// ============================================================
// 二、参数区
// ============================================================
static const int LATTICE_DIM_N      = 128;
static const int MOD_Q              = 3329;
static const int ORBIT_ZONE_COUNT   = 16;
static const int ORBIT_WIDTH_CEH    = 32;
static const int RESPONSE_BOUND     = 260;
static const int PIVOT_WEIGHT       = 8;

// ============================================================
// 三、基础工具
// ============================================================
inline int mod_q(long long x) {
    int r = static_cast<int>(x % MOD_Q);
    if (r < 0) r += MOD_Q;
    return r;
}

inline int centered_q(int x) {
    x = mod_q(x);
    if (x > MOD_Q / 2) x -= MOD_Q;
    return x;
}

template<typename T>
string vec_prefix(const vector<T>& v, int cnt = 8) {
    ostringstream oss;
    int lim = min<int>(cnt, static_cast<int>(v.size()));
    for (int i = 0; i < lim; ++i) {
        if (i) oss << ", ";
        oss << v[i];
    }
    return oss.str();
}

// ============================================================
// 四、SHA256 工具
// ============================================================
vector<uint8_t> sha256_full_bytes(const vector<uint8_t>& input) {
    uint8_t hash[32];
    SHA256(input.data(), input.size(), hash);
    return vector<uint8_t>(hash, hash + 32);
}

uint64_t secure_hash_u64_from_bytes(const vector<uint8_t>& bytes) {
    uint8_t hash[32];
    SHA256(bytes.data(), bytes.size(), hash);

    uint64_t out = 0;
    for (int i = 0; i < 8; ++i) {
        out = (out << 8) | hash[i];
    }
    return out;
}

// ============================================================
// 五、代数核心：负循环卷积
// ============================================================
vector<int> poly_mul_negacyclic(const vector<int>& a, const vector<int>& b) {
    vector<int> result(LATTICE_DIM_N, 0);

    for (int i = 0; i < LATTICE_DIM_N; ++i) {
        if (a[i] == 0) continue;
        for (int j = 0; j < LATTICE_DIM_N; ++j) {
            if (b[j] == 0) continue;

            int k = i + j;
            long long prod = 1LL * a[i] * b[j];

            if (k < LATTICE_DIM_N) {
                result[k] = mod_q(result[k] + prod);
            } else {
                result[k - LATTICE_DIM_N] = mod_q(result[k - LATTICE_DIM_N] - prod);
            }
        }
    }

    return result;
}

// ============================================================
// 六、Orbit 几何头
// ============================================================
struct OrbitHead {
    array<uint64_t, 2> lsh{};
    array<int, ORBIT_ZONE_COUNT> phase{};
};

inline bool ceh_projection_bit(int centered_value, int coeff_index) {
    return (((centered_value / ORBIT_WIDTH_CEH) ^ coeff_index ^ CEH_MAGIC) & 1) != 0;
}

OrbitHead build_orbit_head(const vector<int>& orbitTraceW) {
    OrbitHead head;

    for (int i = 0; i < LATTICE_DIM_N; ++i) {
        int v = centered_q(orbitTraceW[i]);
        bool bit = ceh_projection_bit(v, i);

        if (i < 64) {
            if (bit) head.lsh[0] |= (1ULL << i);
        } else {
            if (bit) head.lsh[1] |= (1ULL << (i - 64));
        }
    }

    int seg_len = LATTICE_DIM_N / ORBIT_ZONE_COUNT;
    for (int seg = 0; seg < ORBIT_ZONE_COUNT; ++seg) {
        int sum = 0;
        for (int i = seg * seg_len; i < (seg + 1) * seg_len; ++i) {
            sum += centered_q(orbitTraceW[i]);
        }
        head.phase[seg] = (sum % 4 + 4) % 4;
    }

    return head;
}

string orbit_head_to_string(const OrbitHead& h) {
    ostringstream oss;
    oss << "LSH=(" << h.lsh[0] << "," << h.lsh[1] << ") Phase=";
    for (int i = 0; i < ORBIT_ZONE_COUNT; ++i) {
        if (i) oss << ",";
        oss << h.phase[i];
    }
    return oss.str();
}

// ============================================================
// 七、Fiat-Shamir 挑战
// ============================================================
vector<int> derive_geometric_pivot(const OrbitHead& orbitHeadRef, const string& messagePayload) {
    vector<uint8_t> stream;

    for (unsigned char ch : CEH_DOMAIN_CHALLENGE) {
        stream.push_back(ch);
    }

    for (const char* p = ARCH_IDENTIFIER; *p; ++p) {
        stream.push_back(static_cast<uint8_t>(*p));
    }

    for (auto block : orbitHeadRef.lsh) {
        for (int i = 0; i < 8; ++i) {
            stream.push_back(static_cast<uint8_t>((block >> (8 * i)) & 0xff));
        }
    }

    for (auto p : orbitHeadRef.phase) {
        stream.push_back(static_cast<uint8_t>(p & 0xff));
    }

    for (unsigned char ch : messagePayload) {
        stream.push_back(ch);
    }

    uint64_t seed = secure_hash_u64_from_bytes(stream);
    mt19937_64 rng(seed);

    vector<int> geometricPivot(LATTICE_DIM_N, 0);
    int placed = 0;

    while (placed < PIVOT_WEIGHT) {
        int pos = static_cast<int>(rng() % LATTICE_DIM_N);
        if (geometricPivot[pos] != 0) continue;

        geometricPivot[pos] = (rng() & 1ULL) ? 1 : (MOD_Q - 1);
        placed++;
    }

    return geometricPivot;
}

// ============================================================
// 八、协议结构体
// ============================================================
struct CEH_KeyPair {
    vector<int> BaseOrbitGen;
    vector<int> CoreVector_S;
    vector<int> PublicOrbit_T;
};

struct CEH_Signature {
    vector<int> EncodedOrbit_Z;
    OrbitHead OrbitHeadRef;
    vector<int> GeometricPivot;
    uint64_t BindAuthHash = 0;
};

struct VerifyResult {
    bool ok = false;
    bool zBoundOk = false;
    bool bindOk = false;
    bool pivotOk = false;
    bool orbitLocked = false;
    int lshDist = 0;
    int phaseDiffSum = 0;
    vector<int> RecoveredOrbit_W;
    OrbitHead RecoveredHead;
};

struct SecurityStats {
    int attackRounds = 0;
    int attackSuccess = 0;
    double collisionRate = 0.0;
    int phaseRepeatCount = 0;
    double meanZ = 0.0;
    double stddevZ = 0.0;
    int lshDensity = 0;
};

// ============================================================
// 九、KeyGen / Sign / Verify
// ============================================================
CEH_KeyPair keygen(mt19937_64& rng) {
    CEH_KeyPair keypair;
    keypair.BaseOrbitGen.resize(LATTICE_DIM_N);
    keypair.CoreVector_S.resize(LATTICE_DIM_N);

    for (int i = 0; i < LATTICE_DIM_N; ++i) {
        keypair.BaseOrbitGen[i] = static_cast<int>(rng() % MOD_Q);

        int r = static_cast<int>(rng() % 12);
        if (r == 0) keypair.CoreVector_S[i] = 1;
        else if (r == 1) keypair.CoreVector_S[i] = -1;
        else keypair.CoreVector_S[i] = 0;
    }

    keypair.PublicOrbit_T = poly_mul_negacyclic(keypair.BaseOrbitGen, keypair.CoreVector_S);
    return keypair;
}

uint64_t build_bind_hash(const vector<int>& encodedOrbitZ,
                         const vector<int>& geometricPivot,
                         const string& messagePayload) {
    vector<uint8_t> bind_bytes;
    bind_bytes.reserve(encodedOrbitZ.size() * 2 +
                       geometricPivot.size() * 2 +
                       messagePayload.size() + 128);

    for (unsigned char ch : CEH_DOMAIN_BIND) {
        bind_bytes.push_back(ch);
    }

    for (const char* p = ARCH_IDENTIFIER; *p; ++p) {
        bind_bytes.push_back(static_cast<uint8_t>(*p));
    }

    for (auto v : encodedOrbitZ) {
        int x = mod_q(v);
        bind_bytes.push_back(static_cast<uint8_t>(x & 0xff));
        bind_bytes.push_back(static_cast<uint8_t>((x >> 8) & 0xff));
    }

    for (auto v : geometricPivot) {
        int x = mod_q(v);
        bind_bytes.push_back(static_cast<uint8_t>(x & 0xff));
        bind_bytes.push_back(static_cast<uint8_t>((x >> 8) & 0xff));
    }

    for (unsigned char ch : messagePayload) {
        bind_bytes.push_back(ch);
    }

    return secure_hash_u64_from_bytes(bind_bytes);
}

CEH_Signature sign(const CEH_KeyPair& keypair,
                   const string& messagePayload,
                   mt19937_64& rng,
                   vector<string>* debugLog = nullptr) {
    while (true) {
        vector<int> stochasticMask(LATTICE_DIM_N);

        for (int i = 0; i < LATTICE_DIM_N; ++i) {
            stochasticMask[i] = static_cast<int>(rng() % 401) - 200;
        }

        vector<int> orbitTraceW = poly_mul_negacyclic(keypair.BaseOrbitGen, stochasticMask);
        OrbitHead orbitHeadRef = build_orbit_head(orbitTraceW);
        vector<int> geometricPivot = derive_geometric_pivot(orbitHeadRef, messagePayload);
        vector<int> pivotSecretProduct = poly_mul_negacyclic(geometricPivot, keypair.CoreVector_S);

        vector<int> encodedOrbitZ(LATTICE_DIM_N);
        int max_abs = 0;

        for (int i = 0; i < LATTICE_DIM_N; ++i) {
            encodedOrbitZ[i] = stochasticMask[i] + centered_q(pivotSecretProduct[i]);
            max_abs = max(max_abs, abs(encodedOrbitZ[i]));
        }

        if (max_abs > RESPONSE_BOUND) continue;

        CEH_Signature signature;
        signature.EncodedOrbit_Z = encodedOrbitZ;
        signature.OrbitHeadRef = orbitHeadRef;
        signature.GeometricPivot = geometricPivot;
        signature.BindAuthHash = build_bind_hash(signature.EncodedOrbit_Z,
                                                 signature.GeometricPivot,
                                                 messagePayload);

        if (debugLog) {
            debugLog->clear();
            debugLog->push_back("[CEH 签名调试]");
            debugLog->push_back("ARCH : " + string(ARCH_IDENTIFIER));
            debugLog->push_back("StochasticMask 前8项 : " + vec_prefix(stochasticMask));
            debugLog->push_back("EncodedOrbit_Z 前8项 : " + vec_prefix(signature.EncodedOrbit_Z));
            debugLog->push_back("OrbitTrace_W 前8项   : " + vec_prefix(orbitTraceW));
            debugLog->push_back("Phase 前8项          : " + vec_prefix(vector<int>(orbitHeadRef.phase.begin(), orbitHeadRef.phase.end())));
            debugLog->push_back("LSH[0]               : " + to_string(orbitHeadRef.lsh[0]));
            debugLog->push_back("LSH[1]               : " + to_string(orbitHeadRef.lsh[1]));
            debugLog->push_back("max|EncodedOrbit_Z|  : " + to_string(max_abs));
            debugLog->push_back("消息长度             : " + to_string(messagePayload.size()));
        }

        return signature;
    }
}

VerifyResult verify(const CEH_KeyPair& keypair,
                    const string& messagePayload,
                    const CEH_Signature& signature) {
    VerifyResult vr;
    vr.zBoundOk = true;

    for (auto v : signature.EncodedOrbit_Z) {
        if (abs(v) > RESPONSE_BOUND) {
            vr.zBoundOk = false;
            vr.ok = false;
            return vr;
        }
    }

    vr.bindOk = (build_bind_hash(signature.EncodedOrbit_Z,
                                 signature.GeometricPivot,
                                 messagePayload) == signature.BindAuthHash);
    if (!vr.bindOk) {
        vr.ok = false;
        return vr;
    }

    vector<int> recoveredAz = poly_mul_negacyclic(keypair.BaseOrbitGen, signature.EncodedOrbit_Z);
    vector<int> recoveredCt = poly_mul_negacyclic(signature.GeometricPivot, keypair.PublicOrbit_T);

    vr.RecoveredOrbit_W.resize(LATTICE_DIM_N);
    for (int i = 0; i < LATTICE_DIM_N; ++i) {
        vr.RecoveredOrbit_W[i] = mod_q(recoveredAz[i] - recoveredCt[i]);
    }

    vr.RecoveredHead = build_orbit_head(vr.RecoveredOrbit_W);
    vector<int> recoveredPivot = derive_geometric_pivot(vr.RecoveredHead, messagePayload);
    vr.pivotOk = (recoveredPivot == signature.GeometricPivot);

    vr.lshDist =
        __builtin_popcountll(signature.OrbitHeadRef.lsh[0] ^ vr.RecoveredHead.lsh[0]) +
        __builtin_popcountll(signature.OrbitHeadRef.lsh[1] ^ vr.RecoveredHead.lsh[1]);

    vr.phaseDiffSum = 0;
    for (int i = 0; i < ORBIT_ZONE_COUNT; ++i) {
        vr.phaseDiffSum += abs(signature.OrbitHeadRef.phase[i] - vr.RecoveredHead.phase[i]);
    }

    vr.orbitLocked = (vr.lshDist == 0 && vr.phaseDiffSum == 0);
    vr.ok = vr.zBoundOk && vr.bindOk && vr.pivotOk && vr.orbitLocked;
    return vr;
}

int attack(const CEH_KeyPair& keypair, const string& messagePayload, int rounds) {
    mt19937_64 rng(1);
    auto signature = sign(keypair, messagePayload, rng, nullptr);

    int success = 0;
    for (int i = 0; i < rounds; ++i) {
        auto fake = signature;
        int idx = static_cast<int>(rng() % LATTICE_DIM_N);
        fake.EncodedOrbit_Z[idx] += (rng() & 1ULL) ? 1 : -1;
        fake.BindAuthHash = build_bind_hash(fake.EncodedOrbit_Z,
                                            fake.GeometricPivot,
                                            messagePayload);

        auto vr = verify(keypair, messagePayload, fake);
        if (vr.ok) success++;
    }
    return success;
}

double head_collision(const CEH_KeyPair& keypair, int trials) {
    mt19937_64 rng(2);
    set<pair<uint64_t, uint64_t>> seen;
    int collisions = 0;

    for (int i = 0; i < trials; ++i) {
        auto sig = sign(keypair, "CEH_COLLISION_TEST", rng, nullptr);
        pair<uint64_t, uint64_t> hh = {sig.OrbitHeadRef.lsh[0], sig.OrbitHeadRef.lsh[1]};
        if (seen.count(hh)) collisions++;
        else seen.insert(hh);
    }
    return static_cast<double>(collisions) / max(1, trials);
}

int phase_repeat_count(const CEH_KeyPair& keypair, const string& messagePayload, int rounds) {
    mt19937_64 rng(3);
    array<int, ORBIT_ZONE_COUNT> basePhase{};
    int same = 0;

    for (int i = 0; i < rounds; ++i) {
        auto sig = sign(keypair, messagePayload, rng, nullptr);
        if (i == 0) {
            basePhase = sig.OrbitHeadRef.phase;
        } else if (sig.OrbitHeadRef.phase == basePhase) {
            same++;
        }
    }
    return same;
}

pair<double,double> stat_z(const vector<int>& encodedOrbitZ) {
    double sum = 0.0;
    for (auto v : encodedOrbitZ) sum += v;
    double mean = sum / encodedOrbitZ.size();

    double var = 0.0;
    for (auto v : encodedOrbitZ) var += (v - mean) * (v - mean);
    var /= encodedOrbitZ.size();

    return {mean, sqrt(var)};
}

int lsh_density(const OrbitHead& head) {
    return __builtin_popcountll(head.lsh[0]) + __builtin_popcountll(head.lsh[1]);
}

SecurityStats collect_stats(const CEH_KeyPair& keypair,
                            const string& messagePayload,
                            const CEH_Signature& signature) {
    SecurityStats s;
    s.attackRounds = 1000;
    s.attackSuccess = attack(keypair, messagePayload, s.attackRounds);
    s.collisionRate = head_collision(keypair, 1000);
    s.phaseRepeatCount = phase_repeat_count(keypair, messagePayload, 200);
    auto [meanZ, stddevZ] = stat_z(signature.EncodedOrbit_Z);
    s.meanZ = meanZ;
    s.stddevZ = stddevZ;
    s.lshDensity = lsh_density(signature.OrbitHeadRef);
    return s;
}

} // namespace CEH_Orbit_V1

// ============================================================
// 十、UI 自定义组件
// ============================================================
class UiCard : public QFrame {
public:
    explicit UiCard(const QString& title, QWidget* parent = nullptr) : QFrame(parent) {
        setStyleSheet(
            "QFrame{background:#1b1d1f;border:1px solid #4c4f52;border-radius:10px;}"
            "QLabel{color:#dadde1;}"
        );
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->setSpacing(6);

        QLabel* titleLabel = new QLabel(title, this);
        titleLabel->setFixedHeight(24);
        titleLabel->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:#f1f3f5;padding-left:2px;}");
        lay->addWidget(titleLabel, 0);

        m_body = new QWidget(this);
        m_body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        m_bodyLayout = new QVBoxLayout(m_body);
        m_bodyLayout->setContentsMargins(0,0,0,0);
        m_bodyLayout->setSpacing(0);

        lay->addWidget(m_body, 1);
    }

    QVBoxLayout* bodyLayout() const { return m_bodyLayout; }

private:
    QWidget* m_body = nullptr;
    QVBoxLayout* m_bodyLayout = nullptr;
};

class LshRenderWidget : public QWidget {
public:
    explicit LshRenderWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(100);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setHead(const CEH_Orbit_V1::OrbitHead& h) {
        m_head = h;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#151719"));

        const int margin = 6;
        const int cols = 32;
        const int rows = 4;

        QRect drawRect = rect().adjusted(margin, margin, -margin, -margin);

        int cellW = max(4, drawRect.width() / cols);
        int cellH = max(8, drawRect.height() / rows);

        p.setPen(Qt::NoPen);

        for (int i = 0; i < 128; ++i) {
            int block = (i < 64) ? 0 : 1;
            int bitIndex = (i < 64) ? i : (i - 64);
            bool bit = ((m_head.lsh[block] >> bitIndex) & 1ULL) != 0;

            int r = i / cols;
            int c = i % cols;

            QRect rc(
                drawRect.left() + c * cellW,
                drawRect.top() + r * cellH,
                cellW - 1,
                cellH - 1
            );

            p.fillRect(rc, bit ? QColor("#00e676") : QColor("#4b4f53"));
        }
    }

private:
    CEH_Orbit_V1::OrbitHead m_head{};
};

class PhaseRenderWidget : public QWidget {
public:
    explicit PhaseRenderWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(120);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setHead(const CEH_Orbit_V1::OrbitHead& h) {
        m_head = h;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#151719"));
        p.setRenderHint(QPainter::Antialiasing);

        const int marginL = 12;
        const int marginR = 12;
        const int marginT = 8;
        const int marginB = 22;

        QRect drawRect = rect().adjusted(marginL, marginT, -marginR, -marginB);

        int n = CEH_Orbit_V1::ORBIT_ZONE_COUNT;
        int barW = max(8, drawRect.width() / n);
        int baseY = drawRect.bottom();
        int usableH = max(20, drawRect.height());

        for (int i = 0; i < n; ++i) {
            int val = m_head.phase[i];
            int h = static_cast<int>((val / 3.0) * usableH);

            QRect bar(
                drawRect.left() + i * barW + 2,
                baseY - h,
                barW - 4,
                h
            );

            p.fillRect(bar, QColor("#00e676"));

            p.setPen(QColor("#dadde1"));
            p.drawText(drawRect.left() + i * barW + 3, rect().bottom() - 4, QString::number(i));
            p.drawText(bar.left() + 2, bar.top() - 2, QString::number(val));
        }
    }

private:
    CEH_Orbit_V1::OrbitHead m_head{};
};

class VectorRenderWidget : public QWidget {
public:
    explicit VectorRenderWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setData(const vector<int>& vec, const QString& title) {
        m_vec = vec;
        m_title = title;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#151719"));
        p.setRenderHint(QPainter::Antialiasing);

        if (m_vec.empty()) return;

        int maxAbs = 1;
        for (int v : m_vec) maxAbs = max(maxAbs, abs(v));

        QRect drawRect = rect().adjusted(38, 8, -12, -22);

        p.setPen(QColor("#4c4f52"));
        p.drawRect(drawRect);
        p.drawLine(drawRect.left(), drawRect.center().y(), drawRect.right(), drawRect.center().y());

        QPainterPath path;
        for (int i = 0; i < static_cast<int>(m_vec.size()); ++i) {
            double x = drawRect.left() +
                       drawRect.width() * double(i) /
                       double(max(1, static_cast<int>(m_vec.size()) - 1));

            double yNorm = m_vec[i] / double(maxAbs);
            double y = drawRect.center().y() - yNorm * (drawRect.height() / 2.15);

            if (i == 0) path.moveTo(x, y);
            else path.lineTo(x, y);
        }

        p.setPen(QPen(QColor("#00e676"), 1.4));
        p.drawPath(path);

        p.setPen(QColor("#dadde1"));
        p.drawText(4, drawRect.top() + 10, QString::number(maxAbs));
        p.drawText(2, drawRect.bottom(), QString::number(-maxAbs));

        if (!m_title.isEmpty()) {
            p.drawText(44, 18, m_title);
        }
    }

private:
    vector<int> m_vec;
    QString m_title;
};

// ============================================================
// 十一、主窗口
// ============================================================
class MainWindow : public QMainWindow {
public:
    MainWindow() {
        setupUi();
        onKeyGen();
        onSign();
        onVerify();
    }

private:
    CEH_Orbit_V1::CEH_KeyPair m_keypair;
    CEH_Orbit_V1::CEH_Signature m_signature;
    CEH_Orbit_V1::VerifyResult m_verifyResult;
    bool m_hasKey = false;
    bool m_hasSig = false;

    uint64_t m_rollingSeed = 0;
    QTimer* realtimeTimer = nullptr;
    bool realtimeRunning = false;

    QLineEdit* messageEdit = nullptr;
    QSpinBox* seedSpin = nullptr;
    QLabel* statusLabel = nullptr;
    QTextEdit* logEdit = nullptr;
    QLabel* miniInfoLabel = nullptr;
    QLabel* realtimeLabel = nullptr;
    QCheckBox* realtimeLogCheck = nullptr;

    LshRenderWidget* lshWidget = nullptr;
    PhaseRenderWidget* phaseWidget = nullptr;
    VectorRenderWidget* zWidget = nullptr;
    VectorRenderWidget* wWidget = nullptr;

    void setupUi() {
        setWindowTitle("CEH-Orbit 协议 调试版");
        resize(1480, 940);

        auto* central = new QWidget(this);
        setCentralWidget(central);

        central->setStyleSheet(R"(
            QWidget { background:#26282a; color:#dadde1; font-size:12px; }
            QLineEdit, QSpinBox, QTextEdit {
                background:#1b1d1f; color:#dadde1; border:1px solid #5f6163; border-radius:6px; padding:6px;
            }
            QPushButton {
                background:#3a3d40; color:#dadde1; border:1px solid #5f6163; border-radius:6px; padding:8px 14px;
            }
            QPushButton:hover { background:#4b4f53; }
            QLabel { color:#dadde1; }
            QCheckBox { color:#dadde1; }
        )");

        auto* root = new QHBoxLayout(central);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(8);

        QWidget* left = new QWidget(this);
        left->setFixedWidth(410);
        auto* leftLayout = new QVBoxLayout(left);
        leftLayout->setContentsMargins(0,0,0,0);
        leftLayout->setSpacing(8);

        auto* configCard = new UiCard("协议参数 / 操作区", this);
        auto* formWrap = new QWidget(this);
        auto* form = new QFormLayout(formWrap);
        form->setContentsMargins(0,0,0,0);
        form->setSpacing(8);

        messageEdit = new QLineEdit("CEH_ORBIT_MESSAGE_BINDING_2026", this);
        seedSpin = new QSpinBox(this);
        seedSpin->setRange(1, 2147483647);
        seedSpin->setValue(20260326);

        form->addRow("消息内容", messageEdit);
        form->addRow("随机种子", seedSpin);
        configCard->bodyLayout()->addWidget(formWrap);

        auto* row1 = new QHBoxLayout();
        QPushButton* keyBtn = new QPushButton("KeyGen", this);
        QPushButton* signBtn = new QPushButton("Sign", this);
        QPushButton* verifyBtn = new QPushButton("Verify", this);
        row1->addWidget(keyBtn);
        row1->addWidget(signBtn);
        row1->addWidget(verifyBtn);
        configCard->bodyLayout()->addLayout(row1);

        auto* row2 = new QHBoxLayout();
        QPushButton* attackBtn = new QPushButton("攻击测试", this);
        QPushButton* autoBtn = new QPushButton("自动演示", this);
        row2->addWidget(attackBtn);
        row2->addWidget(autoBtn);
        configCard->bodyLayout()->addLayout(row2);

        auto* row3 = new QHBoxLayout();
        QPushButton* startRealtimeBtn = new QPushButton("开始实时", this);
        QPushButton* stopRealtimeBtn = new QPushButton("停止实时", this);
        row3->addWidget(startRealtimeBtn);
        row3->addWidget(stopRealtimeBtn);
        configCard->bodyLayout()->addLayout(row3);

        realtimeLabel = new QLabel("实时状态：已停止", this);
        realtimeLogCheck = new QCheckBox("实时模式写日志", this);
        realtimeLogCheck->setChecked(false);

        configCard->bodyLayout()->addWidget(realtimeLabel);
        configCard->bodyLayout()->addWidget(realtimeLogCheck);

        miniInfoLabel = new QLabel(this);
        miniInfoLabel->setWordWrap(true);
        configCard->bodyLayout()->addWidget(miniInfoLabel);

        leftLayout->addWidget(configCard);

        auto* statusCard = new UiCard("状态总览", this);
        statusLabel = new QLabel(this);
        statusLabel->setWordWrap(true);
        statusLabel->setStyleSheet("QLabel{background:#1b1d1f;border:1px solid #5f6163;border-radius:8px;padding:8px;}");
        statusCard->bodyLayout()->addWidget(statusLabel);
        leftLayout->addWidget(statusCard);

        auto* logCard = new UiCard("调试日志", this);
        logEdit = new QTextEdit(this);
        logEdit->setReadOnly(true);
        logEdit->setMinimumHeight(420);
        logCard->bodyLayout()->addWidget(logEdit, 1);
        leftLayout->addWidget(logCard, 1);

        QWidget* right = new QWidget(this);
        auto* rightLayout = new QVBoxLayout(right);
        rightLayout->setContentsMargins(0,0,0,0);
        rightLayout->setSpacing(8);

        auto* topRow = new QHBoxLayout();
        auto* lshCard = new UiCard("LSH 渲染", this);
        lshWidget = new LshRenderWidget(this);
        lshCard->bodyLayout()->addWidget(lshWidget, 1);

        auto* phaseCard = new UiCard("Phase 渲染", this);
        phaseWidget = new PhaseRenderWidget(this);
        phaseCard->bodyLayout()->addWidget(phaseWidget, 1);

        topRow->addWidget(lshCard, 1);
        topRow->addWidget(phaseCard, 1);
        rightLayout->addLayout(topRow, 1);

        auto* zCard = new UiCard("EncodedOrbit_Z 实时波形", this);
        zWidget = new VectorRenderWidget(this);
        zCard->bodyLayout()->addWidget(zWidget, 1);
        rightLayout->addWidget(zCard, 1);

        auto* wCard = new UiCard("RecoveredOrbit_W 实时波形", this);
        wWidget = new VectorRenderWidget(this);
        wCard->bodyLayout()->addWidget(wWidget, 1);
        rightLayout->addWidget(wCard, 1);

        root->addWidget(left, 0);
        root->addWidget(right, 1);

        realtimeTimer = new QTimer(this);
        realtimeTimer->setInterval(120);

        connect(keyBtn, &QPushButton::clicked, this, [this]{ onKeyGen(); });
        connect(signBtn, &QPushButton::clicked, this, [this]{ onSign(); });
        connect(verifyBtn, &QPushButton::clicked, this, [this]{ onVerify(); });
        connect(attackBtn, &QPushButton::clicked, this, [this]{ onAttack(); });
        connect(autoBtn, &QPushButton::clicked, this, [this]{ onAutoDemo(); });
        connect(startRealtimeBtn, &QPushButton::clicked, this, [this]{ startRealtime(); });
        connect(stopRealtimeBtn, &QPushButton::clicked, this, [this]{ stopRealtime(); });
        connect(realtimeTimer, &QTimer::timeout, this, [this]{ onRealtimeTick(); });
    }

    uint64_t currentSeed() const {
        return static_cast<uint64_t>(seedSpin->value());
    }

    string currentMessage() const {
        return messageEdit->text().toStdString();
    }

    void appendLog(const QString& text) {
        logEdit->append(text);
    }

    void startRealtime() {
        if (!m_hasKey) onKeyGen();
        realtimeRunning = true;
        if (m_rollingSeed == 0) m_rollingSeed = currentSeed();
        realtimeTimer->start();
        realtimeLabel->setText("实时状态：运行中");
        appendLog("============================================================");
        appendLog("[实时模式] 已启动");
    }

    void stopRealtime() {
        realtimeRunning = false;
        realtimeTimer->stop();
        realtimeLabel->setText("实时状态：已停止");
        appendLog("============================================================");
        appendLog("[实时模式] 已停止");
    }

    void onRealtimeTick() {
        if (!m_hasKey) return;

        m_rollingSeed++;
        mt19937_64 rng(m_rollingSeed);

        vector<string> debugLog;
        vector<string>* logPtr = realtimeLogCheck->isChecked() ? &debugLog : nullptr;

        m_signature = CEH_Orbit_V1::sign(m_keypair, currentMessage(), rng, logPtr);
        m_hasSig = true;
        m_verifyResult = CEH_Orbit_V1::verify(m_keypair, currentMessage(), m_signature);

        lshWidget->setHead(m_signature.OrbitHeadRef);
        phaseWidget->setHead(m_signature.OrbitHeadRef);
        zWidget->setData(m_signature.EncodedOrbit_Z, "EncodedOrbit_Z");
        wWidget->setData(m_verifyResult.RecoveredOrbit_W, "RecoveredOrbit_W");

        if (realtimeLogCheck->isChecked()) {
            appendLog("============================================================");
            appendLog("[实时刷新]");
            for (const auto& s : debugLog) {
                appendLog(QString::fromStdString(s));
            }
            appendLog(QString("实时 Verify: %1").arg(m_verifyResult.ok ? "PASS" : "FAIL"));
        }

        refreshInfo();
    }

    void onKeyGen() {
        stopRealtime();
        mt19937_64 rng(currentSeed());
        m_keypair = CEH_Orbit_V1::keygen(rng);
        m_hasKey = true;
        m_rollingSeed = currentSeed();

        appendLog("============================================================");
        appendLog("[KeyGen]");
        appendLog("BaseOrbitGen 前8项: " + QString::fromStdString(CEH_Orbit_V1::vec_prefix(m_keypair.BaseOrbitGen)));
        appendLog("CoreVector_S 前8项: " + QString::fromStdString(CEH_Orbit_V1::vec_prefix(m_keypair.CoreVector_S)));
        appendLog("PublicOrbit_T 前8项: " + QString::fromStdString(CEH_Orbit_V1::vec_prefix(m_keypair.PublicOrbit_T)));

        refreshInfo();
    }

    void onSign() {
        if (!m_hasKey) onKeyGen();

        // 关键修复：每次点击 Sign 都滚动 seed，保证图表变化
        if (m_rollingSeed == 0) m_rollingSeed = currentSeed();
        m_rollingSeed++;

        mt19937_64 rng(m_rollingSeed);
        vector<string> debugLog;
        m_signature = CEH_Orbit_V1::sign(m_keypair, currentMessage(), rng, &debugLog);
        m_hasSig = true;

        appendLog("============================================================");
        appendLog(QString("[Sign] 使用种子 = %1").arg(QString::number(m_rollingSeed)));
        for (const auto& s : debugLog) {
            appendLog(QString::fromStdString(s));
        }

        lshWidget->setHead(m_signature.OrbitHeadRef);
        phaseWidget->setHead(m_signature.OrbitHeadRef);
        zWidget->setData(m_signature.EncodedOrbit_Z, "EncodedOrbit_Z");
        refreshInfo();
    }

    void onVerify() {
        if (!m_hasSig) {
            appendLog("[提示] 当前没有签名，请先 Sign");
            return;
        }

        m_verifyResult = CEH_Orbit_V1::verify(m_keypair, currentMessage(), m_signature);
        wWidget->setData(m_verifyResult.RecoveredOrbit_W, "RecoveredOrbit_W");

        appendLog("============================================================");
        appendLog("[Verify]");
        appendLog(QString("z边界检查: %1").arg(m_verifyResult.zBoundOk ? "PASS" : "FAIL"));
        appendLog(QString("BindAuthHash: %1").arg(m_verifyResult.bindOk ? "PASS" : "FAIL"));
        appendLog(QString("Pivot 自洽: %1").arg(m_verifyResult.pivotOk ? "PASS" : "FAIL"));
        appendLog(QString("LSH 汉明距离: %1").arg(m_verifyResult.lshDist));
        appendLog(QString("Phase 累积偏差: %1").arg(m_verifyResult.phaseDiffSum));
        appendLog(QString("轨道锁定: %1").arg(m_verifyResult.orbitLocked ? "PASS" : "FAIL"));
        appendLog(QString("最终结果: %1").arg(m_verifyResult.ok ? "PASS" : "FAIL"));

        refreshInfo();
    }

    void onAttack() {
        if (!m_hasSig) onSign();
        int rounds = 1000;
        int atk = CEH_Orbit_V1::attack(m_keypair, currentMessage(), rounds);

        appendLog("============================================================");
        appendLog("[攻击测试]");
        appendLog(QString("攻击轮数: %1").arg(rounds));
        appendLog(QString("攻击成功: %1").arg(atk));
        appendLog(QString("攻击失败: %1").arg(rounds - atk));

        refreshInfo();
    }

    void onAutoDemo() {
        stopRealtime();
        appendLog("============================================================");
        appendLog("[自动演示] KeyGen -> Sign -> Verify -> Attack");
        onKeyGen();
        onSign();
        onVerify();
        onAttack();
    }

    void refreshInfo() {
        QString st;
        st += QString("协议：%1\n").arg(CEH_Orbit_V1::PROTOCOL_TITLE);
        st += QString("ARCH：%1\n").arg(CEH_Orbit_V1::ARCH_IDENTIFIER);
        st += QString("消息长度：%1\n").arg(messageEdit->text().size());
        st += QString("当前滚动种子：%1\n").arg(QString::number(m_rollingSeed));
        st += QString("Key 状态：%1\n").arg(m_hasKey ? "已生成" : "未生成");
        st += QString("Sign 状态：%1\n").arg(m_hasSig ? "已签名" : "未签名");
        st += QString("实时状态：%1\n").arg(realtimeRunning ? "运行中" : "停止");
        if (m_hasSig) {
            auto vr = CEH_Orbit_V1::verify(m_keypair, currentMessage(), m_signature);
            st += QString("验证状态：%1\n").arg(vr.ok ? "PASS" : "FAIL");
            st += QString("LSH距离：%1\n").arg(vr.lshDist);
            st += QString("Phase偏差：%1\n").arg(vr.phaseDiffSum);
            st += QString("轨道锁：%1\n").arg(vr.orbitLocked ? "已锁定" : "未锁定");
        }
        statusLabel->setText(st);

        if (m_hasSig) {
            auto s = CEH_Orbit_V1::collect_stats(m_keypair, currentMessage(), m_signature);
            QString mini;
            mini += QString("LSH密度: %1/128\n").arg(s.lshDensity);
            mini += QString("mean(z): %1\n").arg(QString::number(s.meanZ, 'f', 3));
            mini += QString("stddev(z): %1\n").arg(QString::number(s.stddevZ, 'f', 3));
            mini += QString("碰撞率: %1\n").arg(QString::number(s.collisionRate, 'f', 6));
            mini += QString("Phase重复: %1/200\n").arg(s.phaseRepeatCount);
            miniInfoLabel->setText(mini);
        } else {
            miniInfoLabel->setText("当前暂无签名数据");
        }
    }
};

// ============================================================
// 十二、Main
// ============================================================
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("CEH-Orbit");
    app.setOrganizationName("ChenEnhua");

    MainWindow w;
    w.show();

    return app.exec();
}