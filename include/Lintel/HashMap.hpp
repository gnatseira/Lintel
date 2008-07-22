/* -*-C++-*- */
/*
   (c) Copyright 2003-2005, Hewlett-Packard Development Company, LP

   See the file named COPYING for license details
*/

/** @file
    A "map" using the HashTable class.  You may want to read the warning in
    HashTable.H about the types of values that you can safely use.
*/

#ifndef LINTEL_HASH_MAP_HPP
#define LINTEL_HASH_MAP_HPP

#if __GNUG__ == 3 && __GNUC_MINOR == 3
#include <ext/stl_hash_fun.h>
#endif
#if __GNUG__ == 3 && __GNUC_MINOR == 4
#include <ext/hash_fun.h>
#endif
#include <functional>

#include <boost/static_assert.hpp>

#include <Lintel/HashTable.hpp>

#include <boost/type_traits/is_integral.hpp>

/// template to hash integral type things; this lets us handle all of
/// the combinations of int64_t, long long, etc., without having to
/// have different variants for all of the sub-types, which is
/// currently impossible because we can't have both long long and
/// int64_t instantiations of the HashMap_hash template.  32bit x86
/// Debian etch thinks those types are the same, so it complains about
/// duplicate templates, but 64bit x86_64 RHEL4 thinks the types are
/// different so requires both of them.  
template<bool isIntegral, int size> struct HashMap_hash_int {
    //    uint32_t operator()(const K &a) const;
};

template <class K> 
struct HashMap_hash 
    : HashMap_hash_int<boost::is_integral<K>::value, sizeof(K)> {
    //    uint32_t operator()(const K &a) const;
};

/// Specialization for std::string
template <> struct HashMap_hash<const std::string> {
    uint32_t operator()(const std::string &a) const {
	return HashTable_hashbytes(a.data(),a.size());
    }
};

/// Specialization for char *
template <> struct HashMap_hash<const char * const> {
    uint32_t operator()(const char * const a) const {
	return HashTable_hashbytes(a,strlen(a));
    }
};

/// To achieve the HashMap_hash_int thing, need to have
/// specializations for the different sizes; this is 4 byte size.
template <> struct HashMap_hash<const uint32_t> {
    uint32_t operator()(const uint32_t _a) const {
	// This turns out to be slow, so turn it back into just using
	// the underlying integer; if someone does put "bad" integers
	// into the system then they can make their own hash function

	// doesn't increase the entropy, but shuffles the entropy across
	// the entire integer, meaning the low bits get shuffled even if they
	// are constant for some reason
//	int a = _a;
//	int b = 0xBEEFCAFE;
//	int ret = 1972;
//	BobJenkinsHashMix(a,b,ret);
//	return ret;
	return static_cast<uint32_t>(_a);
    }
};

/// specialization for 8 byte integers
template <> struct HashMap_hash<const uint64_t> {
    uint32_t operator()(const uint64_t _a) const {
	return BobJenkinsHashMixULL(_a);
    }
};

/// general 4 byte integer hashing
template<> struct HashMap_hash_int<true, 4> : HashMap_hash<const uint32_t> {
    BOOST_STATIC_ASSERT(sizeof(uint32_t) == 4);
};

// general 8 byte integer hashing
template<> struct HashMap_hash_int<true, 8> : HashMap_hash<const uint64_t> {
    BOOST_STATIC_ASSERT(sizeof(uint64_t) == 8);
};

/// Object comparison by pointer, useful for making a hash structure
/// on objects where each object instance is separate.  Instantiated
/// separately rather than as a variant of HashMap_hash because having
/// this as the default behavior doesn't seem safe; people could
/// reasonably expect the comparison to be done as hash(*a) also.
/// Used by HashMap<K, V, PointerHashMapHash<K>,
/// PointerHashMapEqual<K> >
template<typename T> struct PointerHashMapHash {
    uint32_t operator()(const T *a) const {
	BOOST_STATIC_ASSERT(sizeof(a) == 4 || sizeof(a) == 8);
	if (sizeof(a) == 4) {
	    // RHEL4 64bit requires two stage cast even though this
	    // branch should never be executed.
	    return static_cast<uint32_t>(reinterpret_cast<size_t>(a));
	} else if (sizeof(a) == 8) {
	    return BobJenkinsHashMixULL(reinterpret_cast<uint64_t>(a));
	}
    }
};

/// Object equality check -- unnecessary unless operators have been
/// defined.
template<typename T> struct PointerHashMapEqual {
    bool operator()(const T *a, const T *b) const {
	return a == b;
    }
};

/// HashMap class; in our testing almost as fast as the google dense
/// map, but uses almost as little memory as the sparse map.  Note
/// that class K can't be const, see src/tests/hashmap.cpp for
/// details.  An example of extending the HashMap to have a structure
/// as a key are also included in src/tests/hashmap.cpp.
template <class K, class V, 
          class KHash = HashMap_hash<const K>, 
          class KEqual = std::equal_to<const K> >
class HashMap {
public:
    typedef std::pair<K,V> value_type;
    struct value_typeHash {
	KHash khash;
	uint32_t operator()(const value_type &hmv) {
	    return khash(hmv.first);
	}
    };
    struct value_typeEqual {
	KEqual kequal;
	bool operator()(const value_type &a, const value_type &b) {
	    return kequal(a.first,b.first);
	}
    };

    V *lookup(const K &k) {
	value_type fullval; fullval.first = k;
	value_type *v = hashtable.lookup(fullval);
	if (v == NULL) {
	    return NULL;
	} else {
	    return &v->second;
	}
    }

    V &operator[] (const K &k) {
	value_type fullval; fullval.first = k;
	value_type *v = hashtable.lookup(fullval);
	if (v == NULL) {
	    return hashtable.add(fullval)->second;
	} else {
	    return v->second;
	}
    }

    bool exists(const K &k) {
	return lookup(k) != NULL;
    }

    /** returns true if something was removed */
    bool remove(const K &k, bool must_exist = true) {
	value_type fullval; fullval.first = k;
	return hashtable.remove(fullval, must_exist);
    }

    void clear() {
	hashtable.clear();
    }

    uint32_t size() const {
	return hashtable.size();
    }

    bool empty() const {
	return hashtable.empty();
    }

    typedef HashTable<value_type,value_typeHash,value_typeEqual> HashTableT;
    typedef typename HashTableT::iterator iterator;
    typedef typename HashTableT::const_iterator const_iterator;

    iterator begin() {
	return hashtable.begin();
    }
    iterator end() {
	return hashtable.end();
    }

    const_iterator begin() const {
	return hashtable.begin();
    }
    const_iterator end() const {
	return hashtable.end();
    }

    iterator find(const K &k) {
	value_type fullval; fullval.first = k;
	return hashtable.find(fullval);
    }
    
    explicit HashMap(double target_chain_length) 
	: hashtable(target_chain_length)
    { }

    HashMap() {
    }

    HashMap(const HashMap &__in) {
	hashtable = __in.hashtable;
    }

    HashMap &
    operator=(const HashMap &__in) {
	hashtable = __in.hashtable;
	return *this;
    }

    void reserve(uint32_t nentries) {
	hashtable.reserve(nentries);
    }
    
    uint32_t available() {
	return hashtable.available();
    }

    size_t memoryUsage() {
	return hashtable.memoryUsage();
    }

    // primiarily here so that you can get at unsafeGetRawDataVector, with
    // all the caveats that go with that function.
    HashTableT &getHashTable() {
	return hashtable;
    }
private:
    HashTableT hashtable;
};

#endif