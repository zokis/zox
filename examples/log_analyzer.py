import json
import time
import tracemalloc
from collections import Counter

tracemalloc.start()
t0 = time.perf_counter()

# ── dataset ──────────────────────────────────────────────────────────────────
logs = [
    {"method": "GET",    "path": "/api/users",     "status": 200, "ms": 45},
    {"method": "GET",    "path": "/api/users",     "status": 200, "ms": 52},
    {"method": "POST",   "path": "/api/users",     "status": 201, "ms": 120},
    {"method": "GET",    "path": "/api/products",  "status": 200, "ms": 30},
    {"method": "GET",    "path": "/api/products",  "status": 200, "ms": 28},
    {"method": "GET",    "path": "/api/products",  "status": 200, "ms": 35},
    {"method": "PUT",    "path": "/api/users/1",   "status": 200, "ms": 88},
    {"method": "GET",    "path": "/api/orders",    "status": 404, "ms": 12},
    {"method": "POST",   "path": "/api/orders",    "status": 500, "ms": 340},
    {"method": "GET",    "path": "/api/users",     "status": 200, "ms": 41},
    {"method": "GET",    "path": "/api/products",  "status": 200, "ms": 55},
    {"method": "DELETE", "path": "/api/users/2",   "status": 204, "ms": 60},
    {"method": "GET",    "path": "/api/orders",    "status": 500, "ms": 512},
    {"method": "POST",   "path": "/api/auth",      "status": 401, "ms": 15},
    {"method": "POST",   "path": "/api/auth",      "status": 200, "ms": 95},
    {"method": "GET",    "path": "/api/users",     "status": 200, "ms": 48},
    {"method": "GET",    "path": "/api/products",  "status": 200, "ms": 33},
    {"method": "PUT",    "path": "/api/orders/5",  "status": 404, "ms": 10},
    {"method": "GET",    "path": "/api/orders",    "status": 200, "ms": 67},
    {"method": "POST",   "path": "/api/users",     "status": 422, "ms": 25},
]

# ── analise ───────────────────────────────────────────────────────────────────
total    = len(logs)
erros    = [e for e in logs if e["status"] >= 400]
sucessos = [e for e in logs if e["status"] < 400]

tempo_medio   = sum(e["ms"] for e in logs) / total
medio_erro_ms = sum(e["ms"] for e in erros) / len(erros) if erros else 0

paths        = [e["path"] for e in logs]
paths_unicos = list(dict.fromkeys(paths))
path_counts  = Counter(paths)

def status_class(s):
    if s < 300: return "2xx"
    if s < 400: return "3xx"
    if s < 500: return "4xx"
    return "5xx"

classes      = [status_class(e["status"]) for e in logs]
class_counts = Counter(classes)

paths_com_erro = list(dict.fromkeys(e["path"] for e in erros))

relatorio = {
    "total":          total,
    "sucessos":       len(sucessos),
    "erros":          len(erros),
    "tempo_medio_ms": tempo_medio,
    "endpoints":      len(paths_unicos),
}

# ── output ────────────────────────────────────────────────────────────────────
print("=== HTTP Log Analyzer ===\n")
print(f"Total de requisicoes: {total}")
print(f"Sucessos (2xx/3xx): {len(sucessos)}")
print(f"Erros    (4xx/5xx): {len(erros)}")
print(f"Tempo medio (ms):   {tempo_medio}")
print(f"Tempo medio erros:  {medio_erro_ms:.3f}\n")

print("=== Requisicoes por endpoint ===")
for p in paths_unicos:
    print(f"  {p}: {path_counts[p]}")

print("\n=== Distribuicao de status codes ===")
for cls in ["2xx", "3xx", "4xx", "5xx"]:
    print(f"  {cls}: {class_counts[cls]}")

print("\n=== Endpoints com erro ===")
for p in paths_com_erro:
    print(f"  {p}")

print("\n=== Relatorio JSON ===")
print(json.dumps(relatorio))

# ── medicao ───────────────────────────────────────────────────────────────────
t1 = time.perf_counter()
current, peak = tracemalloc.get_traced_memory()
tracemalloc.stop()

print(f"\n=== Performance ===")
print(f"Tempo de execucao: {(t1 - t0) * 1000:.3f} ms")
print(f"Memoria atual:     {current / 1024:.2f} KB")
print(f"Memoria pico:      {peak / 1024:.2f} KB")
