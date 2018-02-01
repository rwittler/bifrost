#ifndef BFG_COLOREDCDBG_HPP
#define BFG_COLOREDCDBG_HPP

#include <iostream>
#include <random>

#include "Color.hpp"
#include "CompactedDBG.hpp"

#define BFG_COLOREDCDBG_FORMAT_VERSION 1

class ColoredCDBG : public CompactedDBG<HashID> {

    friend class HashID;

    public:

        ColoredCDBG(int kmer_length = DEFAULT_K, int minimizer_length = DEFAULT_G);
        ~ColoredCDBG();

        bool build(CDBG_Build_opt& opt);

        bool setColor(const UnitigMap<HashID>& um, size_t color);
        bool joinColors(const UnitigMap<HashID>& um_dest, const UnitigMap<HashID>& um_src);
        ColorSet extractColors(const UnitigMap<HashID>& um) const;

        bool write(const string output_filename, const size_t nb_threads, const bool verbose);

    private:

        void initColorSets(const CDBG_Build_opt& opt, const size_t max_nb_hash = 31);
        void mapColors(const CDBG_Build_opt& opt);
        void checkColors(const CDBG_Build_opt& opt);

        ColorSet* getColorSet(const UnitigMap<HashID>& um);
        const ColorSet* getColorSet(const UnitigMap<HashID>& um) const;

        uint64_t getHash(const UnitigMap<HashID>& um) const;

        uint64_t seeds[256];

        size_t nb_seeds;
        size_t nb_color_sets;

        ColorSet* color_sets;

        KmerHashTable<ColorSet> km_overflow;
};

#endif