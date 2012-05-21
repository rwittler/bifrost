#include "KmerMapper.hpp"
#include <cmath>
#include <iostream>

KmerMapper::~KmerMapper() {
  vector<ContigRef>::iterator it,it_end;
  it_end = contigs.end();
  for (it = contigs.begin(); it != it_end; ++it) {
    if (it->isContig && it->ref.contig != NULL) {
      delete it->ref.contig;
      it->ref.contig = NULL;
    }
  }
}

ContigRef KmerMapper::addContig(const string &s) {
  return addContig(s.c_str());
}

ContigRef KmerMapper::addContig(const char *s) {
  // check that it doesn't map, our responsibility or not?
  ContigRef cr;
  cr.ref.contig = new Contig(s);
   
  uint32_t id = (uint32_t) contigs.size();
  contigs.push_back(cr);

  size_t len = cr.ref.contig->seq.size()-Kmer::k+1;
  bool last = false;
  size_t pos;
  int32_t ipos;
  for (pos = 0; pos < len; pos += stride) {
    if (pos == len-1) {
      last = true;
    }
    Kmer km(s+pos);
    Kmer rep = km.rep();

    ipos  = (km == rep) ? (int32_t) pos : -((int32_t)(pos+Kmer::k-1));
    map.insert(make_pair(rep,ContigRef(id,ipos)));    
  }
  if (!last) {
    pos = len-1;
    Kmer km(s+pos);
    Kmer rep = km.rep();
    ipos  = (km == rep) ? (int32_t) pos : -((int32_t)(pos+Kmer::k-1));
    map.insert(make_pair(rep,ContigRef(id,ipos)));  
  }
  return cr;
}

ContigRef KmerMapper::find(const Kmer km) {
  Kmer rep = km.rep();
  iterator it = map.find(km.rep());
  if (it == map.end()) {
    return ContigRef();
  }
  
  ContigRef cr = it->second;
  assert(!cr.isContig); // cannot be a pointer
  
  ContigRef a = find_rep(cr);
  it->second = a; // shorten the tree for future reference

  return a;
}

// use:  r = m.joinContigs(a,b);
// pre:  a and b are not contig pointers
// post: r is a contigref that points to a newly created contig
//       formed by joining a+b with proper direction, sequences
//       pointed to by a and b have been forwarded to the new contig r

ContigRef KmerMapper::joinContigs(ContigRef a, ContigRef b) {
  //join a to b
  a = find_rep(a);
  uint32_t a_id = a.ref.idpos.id;
  b = find_rep(b);
  uint32_t b_id = b.ref.idpos.id;
  assert(contigs[a_id].isContig && contigs[b_id].isContig);

  // fix this mess
  CompressedSequence &sa = contigs[a_id].ref.contig->seq;
  CompressedSequence &sb = contigs[b_id].ref.contig->seq;

  int direction = 0;
  if(sa.getKmer(sa.size()-Kmer::k) == sb.getKmer(0)) {
    direction = 1;
  }
  if(sa.getKmer(sa.size()-Kmer::k) == sb.getKmer(sb.size()-Kmer::k).twin()) {
    direction = -1;
  }

  assert(direction != 0); // what if we want b+a?

  Contig *joined = new Contig(0); // allocate new contig
  joined->seq.reserveLength(sa.size()+sb.size());
  joined->seq.setSequence(sa,0,sa.size()-Kmer::k + 1,0,true); // copy from a, except matching part, keep orientation of a
  joined->seq.setSequence(sb,sa.size(),sb.size(),0,direction==-1); // copy from b, reverse if neccessary

  ContigRef cr;
  cr.ref.contig = joined;
  uint32_t id = (uint32_t) contigs.size();
  contigs.push_back(cr); // add to contigs set, is now at position id
  
  // invalidated old contigs
  delete contigs[a_id].ref.contig;
  delete contigs[b_id].ref.contig;
  
  contigs[a_id] = ContigRef(id,0);
  contigs[b_id] = ContigRef(id,sa.size()); 

  // TODO: fix stride issues, release k-mers, might improve memory


  return ContigRef(id,0); // points to newly created contig
}


ContigRef KmerMapper::getContig(const size_t id) const {
  ContigRef a = contigs[id];
  if (!a.isContig) {
    a = find_rep(a);
    a = contigs[a.ref.idpos.id];
  }
  return a;
}

ContigRef KmerMapper::getContig(const ContigRef ref) const {
  if (ref.isContig) {
    return ref;
  } else {
    return getContig(ref.ref.idpos.id);
  }
}

void KmerMapper::printContig(const size_t id) {
  if (id >= contigs.size()) {
    cerr << "invalid reference " << id << endl;
  } else {
    ContigRef a = contigs[id];
    if (a.isContig) {
      string s = a.ref.contig->seq.toString();
      cout << "contig " << id << ": length "  << s.size() << endl << s << endl;
      cout << "kmers mapping: " << endl;
      const char *t = s.c_str();
      char tmp[Kmer::MAX_K+1];
      for (int i = 0; i < s.length()-Kmer::k+1; i++) {
	Kmer km(t+i);
	if (!find(km).isEmpty()) {
	  km.rep().toString(tmp);
	  ContigRef km_rep = find(km);
	  cout << string(i,' ') << tmp << " -> (" << km_rep.ref.idpos.id << ", " << km_rep.ref.idpos.pos << ")"  << endl;
	}
      }
    } else {
      ContigRef rep = find_rep(a);
      cout << "-> (" << rep.ref.idpos.id << ", " << rep.ref.idpos.pos << ")" << endl;
    }
  }
}


/* Will we need this at all?
ContigRef KmerMapper::extendContig(ContigRef a, const string &s) {
  
}

ContigRef KmerMapper::extendContig(ContigRef a, const char *s) {
  
}
*/


// use:  r = m.find_rep(a);
// pre:  
// post: if a.isContig is false, then r.isContig is false and
//       m.contig[a.ref.idpos.id].isContig is true, otherwise r == a
// Finds the contigref just before the contig pointer
ContigRef KmerMapper::find_rep(ContigRef a) const {
  uint32_t id;
  ContigRef b = a;
  if (a.isContig) {
    return a;
  }
  int32_t pos = a.ref.idpos.pos;

  while (true) {
    id = a.ref.idpos.id;
    b = contigs[id];
    if (b.isContig) {
      break;
    }

    pos += abs(a.ref.idpos.pos);
    int sign = (pos >= 0) ? 1 : -1;
    sign *= (a.ref.idpos.pos >= 0) ? 1 : -1;
    pos *= sign;
    a = ContigRef(id,pos);    
  }
  return a;
}