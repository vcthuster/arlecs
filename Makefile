# --- CONFIGURATION ---
CC       = clang
# Flags de base (Include path + Warnings)
CFLAGS   = -Iincludes -Wall -Wextra 
# Flags spécifiques
LDFLAGS  = -Llib -larmel -lm # On link Armel et Math (pour le bench galaxy)

# Noms et Chemins
NAME     = arlecs
LIB_OUT  = lib/lib$(NAME).a
SRC      = src/arlecs.c src/arlecs_pool.c
OBJ      = $(SRC:.c=.o)

# Fichiers de Test et Bench
TEST_SRC = tests/arlecs_tests.c
BENCH_SRC= bench.c

# Binaires temporaires
TEST_BIN = tests/test_runner
BENCH_BIN= bench_runner

# --- RÈGLES PRINCIPALES ---

# Par défaut, on compile juste la librairie statique
all: $(LIB_OUT)

# Création de la librairie statique (libarlecs.a)
$(LIB_OUT): $(OBJ)
	@mkdir -p lib
	ar rcs $@ $^
	@echo "📦 Library $(LIB_OUT) created."

# Compilation des objets (.c -> .o)
%.o: %.c
	$(CC) $(CFLAGS) -O3 -c $< -o $@

# --- TESTS (Debug & Sanitize) ---

# Compile et lance les tests en mode DEBUG (O0 + AddressSanitizer pour attraper les fuites/bugs)
tests: $(LIB_OUT)
	@echo "🧪 Compiling Tests (Debug Mode)..."
	$(CC) $(CFLAGS) -O0 -g -fsanitize=address $(TEST_SRC) src/arlecs.c src/arlecs_pool.c -o $(TEST_BIN) $(LDFLAGS)
	@echo "🚀 Running Tests..."
	@./$(TEST_BIN)

# --- BENCHMARK (Performance Max) ---

# Compile et lance le bench en mode RELEASE (O3)
bench: 
	@echo "🏎  Compiling Benchmark (Release -O3)..."
	# Note : On recompile les sources ECS ici avec O3 pour être sûr qu'elles soient inlinées dans le bench
	$(CC) $(CFLAGS) -O3 $(BENCH_SRC) src/arlecs.c src/arlecs_pool.c -o $(BENCH_BIN) $(LDFLAGS)
	@echo "🔥 Running Benchmark..."
	@./$(BENCH_BIN)

# --- NETTOYAGE ---

clean:
	rm -f src/*.o $(LIB_OUT) $(TEST_BIN) $(BENCH_BIN)
	rm -rf *.dSYM

fclean: clean
	rm -f lib/libarlecs.a

re: fclean all

.PHONY: all clean tests bench