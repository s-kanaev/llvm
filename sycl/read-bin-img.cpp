//#include <>

/*
 1. Read sections list. Search for __CLANG__OFFLOAD_BUNDLE__ prefix.
    Outcome: map { section name } => { section info }
 2. Dump binary image descriptor
*/

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <elf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <fstream>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <string_view>

#define ELF_ARCH 64

#if (ELF_ARCH != 64) && (ELF_ARGCH != 32)
#error "Wrong ELF arch"
#endif

#define CONCAT21(a, b) a ## b
#define CONCAT2(a, b) CONCAT21(a, b)

#define CONCAT31(a, b, c) a ## b ## c
#define CONCAT3(a, b, c) CONCAT31(a, b, c)

#define ELF_ADDR_T CONCAT3(Elf, ELF_ARCH, _Addr)
#define ELF_OFF_T CONCAT3(Elf, ELF_ARCH, _Off)
#define ELF_HEADER_T CONCAT3(Elf, ELF_ARCH, _Ehdr)
#define ELF_SECTION_HEADER_T CONCAT3(Elf, ELF_ARCH, _Shdr)

#define CHARPTR(x) reinterpret_cast<char *>(x)

#define READ_(S, V, SZ) \
do {                    \
  S.read(V, SZ);        \
} while (0)

#define READ(S, V) READ_(S, CHARPTR(&V), sizeof(V))

struct Context {
  size_t SectionHeaderOffset;
  size_t NumberOfSectionHeaders;
  size_t NamesSectionHeaderTableIndex;

  std::unique_ptr<char[]> SectionNames;

  std::map<std::string, size_t> NameToIndex;
  std::vector<ELF_SECTION_HEADER_T> SectionHewaders;
};

void readHeaders(std::ifstream &IF, Context &C) {
  ELF_HEADER_T Header;

  READ(IF, Header);

  assert(Header.e_ident[0] == ELFMAG0 && Header.e_ident[1] == ELFMAG1 &&
         Header.e_ident[2] == ELFMAG2 && Header.e_ident[3] == ELFMAG3);

  // TODO validate header

  C.SectionHeaderOffset = Header.e_shoff;
  C.NumberOfSectionHeaders = Header.e_shnum;
  C.NamesSectionHeaderTableIndex = Header.e_shstrndx;

  if (SHN_UNDEF != C.NamesSectionHeaderTableIndex) {
    ELF_SECTION_HEADER_T SectionHeader;
    if (C.NamesSectionHeaderTableIndex == SHN_XINDEX) {
      IF.seekg(SectionHeaderOffset);
      READ(IF, SectionHeader);

      C.NamesSectionHeaderTableIndex = SectionHeader.sh_link;
    }

    IF.seekg(C.SectionHeaderOffset + C.NamesSectionHeaderTableIndex * sizeof(ELF_SECTION_HEADER_T));
    READ(IF, SectionHeader);

    assert(SectionHeader.sh_type == SHT_STRTAB);

    IF.seekg(SectionHeader.sh_offset);
    C.SectionNames.reset(new char[SectionHeader.sh_size]);
    READ_(IF, C.SectionNames.get(), SectionHeader.sh_size);
  }

  C.SectionHeaders.reserve(C.NumberOfSectionHeaders);

  IF.seekg(C.SectionHeaderOffset);

  for (size_t Idx = 0; Idx < C.NumberOfSectionHeaders; ++Idx) {
    ELF_SECTION_HEADER_T SectionHeader;
    READ(IF, SectionHeader);

    // TODO validate section header

    C.SectionHeaders.push_back(SectionHeader);

    if (!SectionHeader.sh_name)
      continue;

    printf("[%zu] - %s\n", Idx, C.SectionNames.get() + SectionHeader.sh_name);

    C.NameToIndex[C.SectionNames.get() + SectionHeader.sh_name] = Idx;
  }
}

int main(int argc, char **argv) {
  // TODO Options
  const char *FName = argv[1];
  std::ifstream IF{FName, std::ios_base::in | std::ios_base::binary};

  const std::string_view WantedSectionName{"__CLANG_OFFLOAD_BUNDLE__sycl-spir64"};

  Context C;

  readHeaders(IF, C);

  auto It = C.NameToIndex.find(WantedSectionName);

  if (C.NameToIndex.end() == It) {
    printf("No section named %s found.\n\tBye!\n", WantedSectionName.data());
    return 0;
  }

  return 0;
}
