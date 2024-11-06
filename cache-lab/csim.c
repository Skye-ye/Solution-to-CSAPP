#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cachelab.h"

#define GET_SET(addr, s, b) (((uint64_t)(addr) >> (b)) & ((1ull << (s)) - 1))
#define GET_TAG(addr, s, b) ((uint64_t)(addr) >> ((s) + (b)))

typedef enum {
  PARAM_NUM,
  PARAM_FILE,
  PARAM_NONE,
} ParamType;

typedef struct {
  char flag;
  ParamType type;
  const char* help;
  void (*handler)(const char*);
} Flag;

typedef struct Node {
  int index;
  struct Node* prev;
  struct Node* next;
} Node;

typedef struct {
  Node* head;
  Node* tail;
  Node* nodes;
} Deque;

typedef struct {
  int valid;
  uint64_t tag;
} Line;

typedef struct {
  Line* lines;
  Deque lru;
  int E;
} Set;

typedef struct {
  Set* sets;
  int S;
  int E;
} Cache;

static int s = 0;
static int E = 0;
static int b = 0;
static char* trace_file = NULL;
static int verbose = 0;

static int hits = 0;
static int misses = 0;
static int evictions = 0;

void print_usage();

void handle_help(const char* value) {
  print_usage();
  exit(0);
}

void handle_verbose(const char* value) {
  (void)value;
  verbose = 1;
}

void handle_s(const char* value) { s = atoi(value); }

void handle_E(const char* value) { E = atoi(value); }

void handle_b(const char* value) { b = atoi(value); }

void handle_tracefile(const char* value) { trace_file = (char*)value; }

Flag flags[] = {{'h', PARAM_NONE, "Print this help message.", print_usage},
                {'v', PARAM_NONE, "Optional verbose flag.", handle_verbose},
                {'s', PARAM_NUM, "Number of set index bits.", handle_s},
                {'E', PARAM_NUM, "Number of lines per set.", handle_E},
                {'b', PARAM_NUM, "Number of block offset bits.", handle_b},
                {'t', PARAM_FILE, "Trace file.", handle_tracefile},
                {'\0', PARAM_NONE, NULL, NULL}};

void print_usage() {
  printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
  printf("Options:\n");
  Flag* flag = flags;
  while (flag->flag) {
    switch (flag->type) {
      case PARAM_NONE:
        printf("  -%c         %s\n", flag->flag, flag->help);
        break;
      case PARAM_NUM:
        printf("  -%c <num>   %s\n", flag->flag, flag->help);
        break;
      case PARAM_FILE:
        printf("  -%c <file>  %s\n", flag->flag, flag->help);
        break;
    }
    flag++;
  }
  printf("\n");
  printf(
      "Examples:\n"
      "  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n"
      "  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}
Flag* find_flag(char flag_char) {
  for (Flag* flag = flags; flag->flag != '\0'; flag++) {
    if (flag->flag == flag_char) {
      return flag;
    }
  }
  return NULL;
}

int parse_args(int argc, char* argv[]) {
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-' || argv[i][1] == '\0') {
      printf("./csim: Expected flag, got '%s'\n", argv[i]);
      print_usage();
      return 1;
    }

    Flag* flag = find_flag(argv[i][1]);
    if (!flag) {
      printf("./csim: Unknown flag '-%c'\n", argv[i][1]);
      print_usage();
      return 1;
    }

    if (flag->type != PARAM_NONE) {
      if (i + 1 >= argc) {
        printf("./csim: option requires an argument -- '%c'\n", argv[i][1]);
        print_usage();
        return 1;
      }
      flag->handler(argv[++i]);
    } else {
      flag->handler(NULL);
    }
  }

  if (!trace_file || !s || !E || !b) {
    printf("./csim: Missing required command line argument\n");
    print_usage();
    return 1;
  }

  return 0;
}

void init_deque(Set* set) {
  set->lru.nodes = (Node*)malloc(set->E * sizeof(Node));
  set->lru.head = NULL;
  set->lru.tail = NULL;

  for (int i = 0; i < set->E; i++) {
    set->lru.nodes[i].index = i;
    set->lru.nodes[i].prev = NULL;
    set->lru.nodes[i].next = NULL;
  }
}

void free_deque(Set* set) { free(set->lru.nodes); }

void deque_push(Deque* deque, int index) {
  Node* node = &deque->nodes[index];
  node->prev = deque->tail;
  node->next = NULL;

  if (deque->tail) {
    deque->tail->next = node;
  } else {
    deque->head = node;
  }
  deque->tail = node;
}

void deque_remove(Deque* deque, int index) {
  Node* node = &deque->nodes[index];

  if (node->prev) {
    node->prev->next = node->next;
  } else {
    deque->head = node->next;
  }

  if (node->next) {
    node->next->prev = node->prev;
  } else {
    deque->tail = node->prev;
  }

  node->prev = NULL;
  node->next = NULL;
}

int deque_get_lru(Deque* deque) { return deque->head->index; }

Cache* create_cache(int s, int E) {
  Cache* cache = (Cache*)malloc(sizeof(Cache));
  if (!cache) return NULL;

  cache->S = 1 << s;
  cache->E = E;

  cache->sets = (Set*)malloc(cache->S * sizeof(Set));
  if (!cache->sets) {
    free(cache);
    return NULL;
  }

  for (int i = 0; i < cache->S; i++) {
    cache->sets[i].lines = (Line*)malloc(E * sizeof(Line));
    cache->sets[i].E = E;

    if (!cache->sets[i].lines) {
      for (int j = 0; j < i; j++) {
        free_deque(&cache->sets[j]);
        free(cache->sets[j].lines);
      }
      free(cache->sets);
      free(cache);
      return NULL;
    }

    for (int j = 0; j < E; j++) {
      cache->sets[i].lines[j].valid = 0;
      cache->sets[i].lines[j].tag = 0;
    }
    init_deque(&cache->sets[i]);
  }

  return cache;
}

void free_cache(Cache* cache) {
  if (!cache) return;

  for (int i = 0; i < cache->S; i++) {
    free_deque(&cache->sets[i]);
    free(cache->sets[i].lines);
  }
  free(cache->sets);
  free(cache);
}

void access_cache(Cache* cache, uint64_t addr) {
  uint64_t set_index = GET_SET(addr, s, b);
  uint64_t tag = GET_TAG(addr, s, b);

  Set* set = &cache->sets[set_index];

  for (int i = 0; i < set->E; i++) {
    if (set->lines[i].valid && set->lines[i].tag == tag) {
      hits++;
      if (verbose) {
        printf(" hit");
      }

      deque_remove(&set->lru, i);
      deque_push(&set->lru, i);
      return;
    }
  }

  misses++;
  if (verbose) {
    printf(" miss");
  }

  int empty_index = -1;
  for (int i = 0; i < set->E; i++) {
    if (!set->lines[i].valid) {
      empty_index = i;
      break;
    }
  }

  if (empty_index != -1) {
    set->lines[empty_index].valid = 1;
    set->lines[empty_index].tag = tag;
    deque_push(&set->lru, empty_index);
  } else {
    evictions++;
    if (verbose) {
      printf(" eviction");
    }

    int lru_index = deque_get_lru(&set->lru);
    set->lines[lru_index].tag = tag;
    deque_remove(&set->lru, lru_index);
    deque_push(&set->lru, lru_index);
  }
}

int parse(const char* line, uint64_t* addr, int* size) {
  if (line[0] != ' ') {
    return 0;
  }

  if (sscanf(line + 2, "%lx,%d", addr, size) == 2) {
    return 1;
  }

  return 0;
}

int main(int argc, char* argv[]) {
  if (argc == 1) {
    printf("./csim: Missing required command line argument\n");
    print_usage();
  }

  if (parse_args(argc, argv) != 0) {
    return 1;
  }

  Cache* cache = create_cache(s, E);
  if (!cache) {
    printf("Failed to create cache\n");
    return 1;
  }

  FILE* trace = fopen(trace_file, "r");
  if (!trace) {
    printf("Failed to open trace file\n");
    free_cache(cache);
    return 1;
  }

  char line[256];
  while (fgets(line, sizeof(line), trace)) {
    uint64_t addr;
    int size;

    if (parse(line, &addr, &size)) {
      if (verbose) {
        line[strlen(line) - 1] = '\0';
        printf("%s", line + 1);
      }
      if (line[1] == 'M') {
        access_cache(cache, addr);
      }
      access_cache(cache, addr);
      if (verbose) {
        printf("\n");
      }
    }
  }
  printSummary(hits, misses, evictions);
  free_cache(cache);
  fclose(trace);
  return 0;
}