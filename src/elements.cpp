// Copyright 2020 Chia Network Inc

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//    http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstring>
#include <string.h>

#include "bls.hpp"

namespace bls {

const size_t G1Element::SIZE;

G1Element G1Element::FromBytes(Bytes const bytes) {
    G1Element ele = G1Element::FromBytesUnchecked(bytes);
    ele.CheckValid();
    return ele;
}

G1Element G1Element::FromBytesUnchecked(Bytes const bytes)
{
    if (bytes.size() != SIZE) {
        throw std::invalid_argument("G1Element::FromBytes: Invalid size");
    }

    G1Element ele;

    // convert bytes to relic form
    uint8_t buffer[G1Element::SIZE + 1];
    std::memcpy(buffer + 1, bytes.begin(), G1Element::SIZE);
    buffer[0] = 0x00;
    buffer[1] &= 0x1f;  // erase 3 msbs from given input

    bool fZerosOnly = Util::HasOnlyZeros(Bytes(buffer, G1Element::SIZE + 1));
    if ((bytes[0] & 0xc0) == 0xc0) {  // representing infinity
        // enforce that infinity must be 0xc0000..00
        if (bytes[0] != 0xc0 || !fZerosOnly) {
            throw std::invalid_argument("Given G1 infinity element must be canonical");
        }
        return ele;
    } else {
        if ((bytes[0] & 0xc0) != 0x80) {
            throw std::invalid_argument(
                "Given G1 non-infinity element must start with 0b10");
        }

        if (fZerosOnly) {
            throw std::invalid_argument("G1 non-infinity element can't have only zeros");
        }

        if (bytes[0] & 0x20) {  // sign bit
            buffer[0] = 0x03;
        } else {
            buffer[0] = 0x02;
        }
    }
    g1_read_bin(ele.p, buffer, G1Element::SIZE + 1);
    return ele;
}

G1Element G1Element::FromByteVector(const std::vector<uint8_t>& bytevec)
{
    return G1Element::FromBytes(Bytes(bytevec));
}

G1Element G1Element::FromNative(const blst_p1 element)
{
    G1Element ele;
    memcpy(&(ele.p), &element, sizeof(blst_p1));
    return ele;
}

G1Element G1Element::FromMessage(const std::vector<uint8_t>& message,
                                 const uint8_t* dst,
                                 int dst_len)
{
    return FromMessage(Bytes(message), dst, dst_len);
}

G1Element G1Element::FromMessage(Bytes const message,
                                 const uint8_t* dst,
                                 int dst_len)
{
    G1Element ans;
    ep_map_dst(ans.p, message.begin(), (int)message.size(), dst, dst_len);
    assert(ans.IsValid());
    return ans;
}

G1Element G1Element::Generator()
{

    G1Element ele;
    const blst_p1 *gen1 = blst_p1_generator();
    ele.FromNative(*gen1);
    return ele;
}

bool G1Element::IsValid() const {
    // Infinity no longer valid in Relic
    // https://github.com/relic-toolkit/relic/commit/f3be2babb955cf9f82743e0ae5ef265d3da6c02b
    if (blst_p1_is_inf(&p) == 1)
        return true;

    return blst_p1_on_curve((blst_p1*)&p);
}

void G1Element::CheckValid() const {
    if (!IsValid())
        throw std::invalid_argument("G1 element is invalid");
}

void G1Element::ToNative(blst_p1 *output) const {
    memcpy(output, &p, sizeof(blst_p1));
}

G1Element G1Element::Negate() const
{
    G1Element ans;
    ans.FromNative(p);
    blst_p1_cneg(&(ans.p), true);
    return ans;
}

GTElement G1Element::Pair(const G2Element& b) const { return (*this) & b; }

uint32_t G1Element::GetFingerprint() const
{
    uint8_t buffer[G1Element::SIZE];
    uint8_t hash[32];
    memcpy(buffer, Serialize().data(), G1Element::SIZE);
    Util::Hash256(hash, buffer, G1Element::SIZE);
    return Util::FourBytesToInt(hash);
}

std::vector<uint8_t> G1Element::Serialize() const {
    uint8_t buffer[G1Element::SIZE + 1];
    g1_write_bin(buffer, G1Element::SIZE + 1, p, 1);

    if (buffer[0] == 0x00) {  // infinity
        std::vector<uint8_t> result(G1Element::SIZE, 0);
        result[0] = 0xc0;
        return result;
    }

    if (buffer[0] == 0x03) {  // sign bit set
        buffer[1] |= 0x20;
    }

    buffer[1] |= 0x80;  // indicate compression
    return std::vector<uint8_t>(buffer + 1, buffer + 1 + G1Element::SIZE);
}

bool operator==(const G1Element & a, const G1Element &b)
{
    return memcmp(&(a.p), &(b.p), sizeof(blst_p1)) == 0;
}

bool operator!=(const G1Element & a, const G1Element & b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, const G1Element &ele)
{
    return os << Util::HexStr(ele.Serialize());
}

G1Element& operator+=(G1Element& a, const G1Element& b)
{
    blst_p1_add(&(a.p), &(a.p), &(b.p));
    return a;
}

G1Element operator+(const G1Element& a, const G1Element& b)
{
    G1Element ans;
    blst_p1_add(&(ans.p), &(a.p), &(b.p));
    return ans;
}

G1Element operator*(const G1Element& a, const blst_scalar& k)
{
    G1Element ans;
    byte *bte = Util::SecAlloc<byte>(32);
    blst_lendian_from_scalar(bte, &k);
    blst_p1_mult(&(ans.p), &(a.p), bte, 256);
    Util::SecFree(bte);

    return ans;
}

G1Element operator*(const blst_scalar& k, const G1Element& a) { return a * k; }



// G2Element definitions below



const size_t G2Element::SIZE;

G2Element G2Element::FromBytes(Bytes const bytes) {
    G2Element ele = G2Element::FromBytesUnchecked(bytes);
    ele.CheckValid();
    return ele;
}

G2Element G2Element::FromBytesUnchecked(Bytes const bytes)
{
    if (bytes.size() != SIZE) {
        throw std::invalid_argument("G2Element::FromBytes: Invalid size");
    }

    G2Element ele;
    uint8_t buffer[G2Element::SIZE + 1];
    std::memcpy(buffer + 1, bytes.begin() + G2Element::SIZE / 2, G2Element::SIZE / 2);
    std::memcpy(buffer + 1 + G2Element::SIZE / 2, bytes.begin(), G2Element::SIZE / 2);
    buffer[0] = 0x00;
    buffer[49] &= 0x1f;  // erase 3 msbs from input

    if ((bytes[48] & 0xe0) != 0x00) {
        throw std::invalid_argument(
            "Given G2 element must always have 48th byte start with 0b000");
    }
    bool fZerosOnly = Util::HasOnlyZeros(Bytes(buffer, G2Element::SIZE + 1));
    if (((bytes[0] & 0xc0) == 0xc0)) {  // infinity
        // enforce that infinity must be 0xc0000..00
        if (bytes[0] != 0xc0 || !fZerosOnly) {
            throw std::invalid_argument(
                "Given G2 infinity element must be canonical");
        }
        return ele;
    } else {
        if (((bytes[0] & 0xc0) != 0x80)) {
            throw std::invalid_argument(
                "G2 non-inf element must have 0th byte start with 0b10");
        }

        if (fZerosOnly) {
            throw std::invalid_argument("G2 non-infinity element can't have only zeros");
        }

        if (bytes[0] & 0x20) {
            buffer[0] = 0x03;
        } else {
            buffer[0] = 0x02;
        }
    }

    g2_read_bin(ele.q, buffer, G2Element::SIZE + 1);
    return ele;
}

G2Element G2Element::FromByteVector(const std::vector<uint8_t>& bytevec)
{
    return G2Element::FromBytes(Bytes(bytevec));
}

G2Element G2Element::FromNative(const blst_p2 element)
{
    G2Element ele;
    memcpy(&(ele.q), &element, sizeof(blst_p2));
    return ele;
}

G2Element G2Element::FromMessage(const std::vector<uint8_t>& message,
                                 const uint8_t* dst,
                                 int dst_len)
{
    return FromMessage(Bytes(message), dst, dst_len);
}

G2Element G2Element::FromMessage(Bytes const message,
                                 const uint8_t* dst,
                                 int dst_len)
{
    G2Element ans;
    ep2_map_dst(ans.q, message.begin(), (int)message.size(), dst, dst_len);
    assert(ans.IsValid());
    return ans;
}

G2Element G2Element::Generator()
{
    G2Element ele;
    const blst_p2 *gen2 = blst_p2_generator();
    ele.FromNative(*gen2);
    return ele;
}

bool G2Element::IsValid() const {
    // Infinity no longer valid in Relic
    // https://github.com/relic-toolkit/relic/commit/f3be2babb955cf9f82743e0ae5ef265d3da6c02b
    if (blst_p2_is_inf(&q) == 1)
        return true;

    return blst_p2_on_curve((blst_p2*)&q);
}

void G2Element::CheckValid() const {
    if (!IsValid())
        throw std::invalid_argument("G2 element is invalid");
}

void G2Element::ToNative(blst_p2 *output) const {
    memcpy(output, (blst_p2*)&q, sizeof(blst_p2));
}

G2Element G2Element::Negate() const
{
    G2Element ans;
    ans.FromNative(q);
    blst_p2_cneg(&(ans.q), true);
    return ans;
}

GTElement G2Element::Pair(const G1Element& a) const { return a & (*this); }

std::vector<uint8_t> G2Element::Serialize() const {
    uint8_t buffer[G2Element::SIZE + 1];
    g2_write_bin(buffer, G2Element::SIZE + 1, (blst_p2*)q, 1);

    if (buffer[0] == 0x00) {  // infinity
        std::vector<uint8_t> result(G2Element::SIZE, 0);
        result[0] = 0xc0;
        return result;
    }

    // remove leading 3 bits
    buffer[1] &= 0x1f;
    buffer[49] &= 0x1f;
    if (buffer[0] == 0x03) {
        buffer[49] |= 0xa0;  // swapped later to 0
    } else {
        buffer[49] |= 0x80;
    }

    // Swap buffer, relic uses the opposite ordering for Fq2 elements
    std::vector<uint8_t> result(G2Element::SIZE, 0);
    std::memcpy(result.data(), buffer + 1 + G2Element::SIZE / 2, G2Element::SIZE / 2);
    std::memcpy(result.data() + G2Element::SIZE / 2, buffer + 1, G2Element::SIZE / 2);
    return result;
}

bool operator==(G2Element const& a, G2Element const& b)
{
    return memcpy((blst_p2*)&(a.q), (blst_p2*)&(b.q), sizeof(blst_p2)) == 0;
}

bool operator!=(G2Element const& a, G2Element const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, const G2Element & s)
{
    return os << Util::HexStr(s.Serialize());
}

G2Element& operator+=(G2Element& a, const G2Element& b)
{
    blst_p2_add(&(a.q), &(a.q), &(b.q));
    return a;
}

G2Element operator+(const G2Element& a, const G2Element& b)
{
    G2Element ans;
    blst_p2_add(&(ans.q), &(a.q), &(b.q));
    return ans;
}

G2Element operator*(const G2Element& a, const blst_scalar& k)
{
    G2Element ans;
    byte *bte = Util::SecAlloc<byte>(32);
    blst_lendian_from_scalar(bte, &k);
    blst_p2_mult(&(ans.q), &(a.q), bte, 256);
    Util::SecFree(bte);
     
    return ans;
}

G2Element operator*(const blst_scalar& k, const G2Element& a) { return a * k; }



// GTElement

const size_t GTElement::SIZE;

GTElement GTElement::FromBytes(Bytes const bytes)
{
    GTElement ele = GTElement::FromBytesUnchecked(bytes);
    if (!blst_fp12_in_group(&(ele.r)))
        throw std::invalid_argument("GTElement is invalid");
    return ele;
}

GTElement GTElement::FromBytesUnchecked(Bytes const bytes)
{
    if (bytes.size() != SIZE) {
        throw std::invalid_argument("GTElement::FromBytes: Invalid size");
    }
    GTElement ele = GTElement();
    gt_read_bin(ele.r, bytes.begin(), GTElement::SIZE);
    return ele;
}

GTElement GTElement::FromByteVector(const std::vector<uint8_t>& bytevec)
{
    return GTElement::FromBytes(Bytes(bytevec));
}

GTElement GTElement::FromNative(const blst_fp12 *element)
{
    GTElement ele = GTElement();
    memcpy(&(ele.r), element, sizeof(blst_fp12));
    return ele;
}

GTElement GTElement::Unity() {
    GTElement ele = GTElement();
    gt_set_unity(ele.r);
    return ele;
}


bool operator==(GTElement const& a, GTElement const& b)
{
    return memcmp(&(a.r), &(b.r), sizeof(blst_fp12)) == 0;
}

bool operator!=(GTElement const& a, GTElement const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, GTElement const& ele)
{
    return os << Util::HexStr(ele.Serialize());
}

GTElement operator&(const G1Element& a, const G2Element& b)
{
    G1Element nonConstA(a);
    blst_fp12 ans;
    blst_p2 tmp;
    b.ToNative(&tmp);
    pp_map_oatep_k12(ans, nonConstA.p, tmp);
    GTElement ret = GTElement::FromNative(&ans);
    return ret;
}

GTElement operator*(GTElement& a, GTElement& b)
{
    GTElement ans;
    fp12_mul(ans.r, a.r, b.r);
    return ans;
}

void GTElement::Serialize(uint8_t* buffer) const
{
    gt_write_bin(buffer, GTElement::SIZE, *(blst_fp12 *)&r, 1);
}

std::vector<uint8_t> GTElement::Serialize() const
{
    std::vector<uint8_t> data(GTElement::SIZE);
    Serialize(data.data());
    return data;
}

}  // end namespace bls
