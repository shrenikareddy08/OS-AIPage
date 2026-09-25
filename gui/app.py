import json
from flask import Flask, render_template, jsonify, request
import csv
import os
import time
import uuid
import sqlite3
import signal
import subprocess
from collections import deque

app = Flask(__name__)

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CSV_FILE = os.path.join(
    BASE_DIR,
    "results",
    "v21_three_way_comparison.csv"
)

# ---------------------------------------------------------
# AIPage runtime state
# ---------------------------------------------------------

SERVER_START_TIME = time.time()
LIVE_PIPELINE = None
LIVE_PIPELINE_LOG = os.path.join(BASE_DIR, "results", "live_pipeline.log")


def live_pipeline_running():
    return LIVE_PIPELINE is not None and LIVE_PIPELINE.poll() is None


def set_live_state_running(running):
    path = os.path.join(BASE_DIR, "results", "live_state.json")
    try:
        state = {}
        if os.path.exists(path):
            with open(path, "r") as f:
                state = json.load(f)
        state["running"] = bool(running)
        state["timestamp"] = time.time()
        tmp = path + ".tmp"
        with open(tmp, "w") as f:
            json.dump(state, f, indent=2)
        os.replace(tmp, path)
    except (OSError, ValueError, json.JSONDecodeError):
        pass


def get_monitor_pid():
    """
    Return a valid process PID for Linux /proc monitoring.

    Priority:
    1. Explicit AIPAGE_MONITOR_PID if it currently exists.
    2. A running AIPage process.
    3. Flask's own PID as a safe fallback.
    """

    configured = os.environ.get("AIPAGE_MONITOR_PID")

    if configured:
        try:
            pid = int(configured)

            if pid > 0 and os.path.exists(f"/proc/{pid}"):
                return pid

        except (ValueError, TypeError):
            pass

    preferred_names = {
        "realtime_ai_FINAL",
        "realtime_ai",
        "real_memory_workload",
        "real_memory_monitor",
        "AIPage_FINAL",
        "AIPage_FINAL_SUBMISSION",
    }

    try:
        for entry in os.listdir("/proc"):

            if not entry.isdigit():
                continue

            pid = int(entry)

            try:
                with open(f"/proc/{pid}/comm", "r") as f:
                    name = f.read().strip()

                if name in preferred_names:
                    return pid

            except (FileNotFoundError, PermissionError, OSError):
                continue

    except Exception:
        pass

    return os.getpid()


MONITOR_PID = get_monitor_pid()


PREVIOUS_SYSTEM_CPU = None
PREVIOUS_SYSTEM_TIME = None

CLIENTS = {}

ACCESS_HISTORY = deque(maxlen=100)

# Live workload telemetry
LIVE_TRACE_FILE = os.path.join(
    BASE_DIR,
    "results",
    f"live_access_{MONITOR_PID}.csv"
)

LIVE_TRACE_OFFSET = 0
LIVE_TRACE_INITIALIZED = False

CURRENT_STATE = {
    "current_page": None,
    "predicted_page": None,
    "confidence": 0.0,
    "victim_page": None,
    "page_faults": 0,
    "page_hits": 0
}


# ---------------------------------------------------------
# Live workload page-access reader
# ---------------------------------------------------------

def read_live_page_accesses():

    global LIVE_TRACE_OFFSET
    global LIVE_TRACE_INITIALIZED

    if not os.path.exists(LIVE_TRACE_FILE):
        return

    try:

        if not LIVE_TRACE_INITIALIZED:

            with open(LIVE_TRACE_FILE, "r") as f:
                lines = f.readlines()

            for line in lines[-100:]:

                parts = line.strip().split(",")

                if len(parts) >= 3:

                    try:
                        page = int(parts[2])
                        ACCESS_HISTORY.append(page)
                    except ValueError:
                        pass

            LIVE_TRACE_OFFSET = os.path.getsize(
                LIVE_TRACE_FILE
            )

            LIVE_TRACE_INITIALIZED = True

            if ACCESS_HISTORY:
                CURRENT_STATE["current_page"] = ACCESS_HISTORY[-1]

            return

        with open(LIVE_TRACE_FILE, "r") as f:

            f.seek(LIVE_TRACE_OFFSET)
            new_data = f.read()
            LIVE_TRACE_OFFSET = f.tell()

        for line in new_data.splitlines():

            parts = line.strip().split(",")

            if len(parts) < 3:
                continue

            try:
                page = int(parts[2])
            except ValueError:
                continue

            ACCESS_HISTORY.append(page)
            CURRENT_STATE["current_page"] = page

    except Exception as error:

        print("Live trace reader error:", error)


# ---------------------------------------------------------
# Benchmark results
# ---------------------------------------------------------

def load_results():

    rows = []

    try:

        with open(CSV_FILE, newline="") as f:

            reader = csv.DictReader(f)

            for row in reader:

                rows.append({
                    "workload": row["workload"],
                    "frames": int(row["frames"]),
                    "accesses": int(row["accesses"]),

                    "ai_hit_rate":
                        float(row["ai_hit_rate"]),

                    "lru_hit_rate":
                        float(row["lru_hit_rate"]),

                    "fifo_hit_rate":
                        float(row["fifo_hit_rate"]),

                    "ai_faults":
                        int(row["ai_faults"]),

                    "lru_faults":
                        int(row["lru_faults"]),

                    "fifo_faults":
                        int(row["fifo_faults"])
                })

    except Exception as error:

        print("Unable to load benchmark results:", error)

    return rows


# ---------------------------------------------------------
# Live AIPage state
# ---------------------------------------------------------

LIVE_STATE_FILE = os.path.join(
    BASE_DIR,
    "results",
    "live_state.json"
)


def load_live_state():

    default_state = {
        "running": False,
        "access_count": 0,
        "current_page": None,
        "predicted_page": None,
        "confidence": 0.0,
        "recent_pages": [],
        "resident_frames": [],
        "hit_fault": "UNKNOWN",
        "victim_page": None,
        "decision": "WAITING",
        "reason": "Waiting for AIPage",
        "page_hits": 0,
        "page_faults": 0,
        "replacements": 0,
        "prediction_count": 0,
        "prediction_correct": 0,
        "hit_rate": 0.0,
        "fault_rate": 0.0,
        "prediction_accuracy": 0.0
    }

    try:

        with open(LIVE_STATE_FILE) as f:
            data = json.load(f)

        default_state.update(data)

    except Exception as error:

        print("Unable to load live state:", error)

    return default_state


# ---------------------------------------------------------
# Linux /proc monitoring
# ---------------------------------------------------------

def read_process_info(pid=None):

    if pid is None:
        pid = MONITOR_PID

    status_file = f"/proc/{pid}/status"
    stat_file = f"/proc/{pid}/stat"

    process_name = "--"
    process_state = "--"

    vm_size = 0
    vm_rss = 0
    minor_faults = 0
    major_faults = 0

    try:

        with open(status_file) as f:

            for line in f:

                if line.startswith("Name:"):
                    process_name = line.split(":", 1)[1].strip()

                elif line.startswith("State:"):
                    process_state = line.split(":", 1)[1].strip()

                elif line.startswith("VmSize:"):
                    vm_size = int(line.split()[1])

                elif line.startswith("VmRSS:"):
                    vm_rss = int(line.split()[1])

        with open(stat_file) as f:

            data = f.read().split()

            minor_faults = int(data[9])
            major_faults = int(data[11])

    except Exception as error:

        print("Process monitor error:", error)

    return {
        "pid": pid,
        "name": process_name,
        "state": process_state,
        "vm_size_kb": vm_size,
        "vm_rss_kb": vm_rss,
        "minor_faults": minor_faults,
        "major_faults": major_faults,
        "timestamp": time.strftime("%H:%M:%S")
    }


# ---------------------------------------------------------
# System CPU information
# ---------------------------------------------------------

def read_system_cpu():
    global PREVIOUS_SYSTEM_CPU
    global PREVIOUS_SYSTEM_TIME

    try:
        with open("/proc/stat") as f:
            line = f.readline()

        values = line.split()

        if not values or values[0] != "cpu":
            return 0.0

        times = [int(x) for x in values[1:]]
        total = sum(times)

        idle = times[3]

        if PREVIOUS_SYSTEM_CPU is None:
            PREVIOUS_SYSTEM_CPU = idle
            PREVIOUS_SYSTEM_TIME = total
            return 0.0

        delta_idle = idle - PREVIOUS_SYSTEM_CPU
        delta_total = total - PREVIOUS_SYSTEM_TIME

        PREVIOUS_SYSTEM_CPU = idle
        PREVIOUS_SYSTEM_TIME = total

        if delta_total <= 0:
            return 0.0

        usage = (
            (delta_total - delta_idle)
            / delta_total
        ) * 100.0

        return round(usage, 2)

    except Exception:
        return 0.0


# ---------------------------------------------------------
# System information
# ---------------------------------------------------------

def read_system_info(pid=None):

    memory = {
        "total": 0,
        "available": 0
    }

    try:

        with open("/proc/meminfo") as f:

            for line in f:

                if line.startswith("MemTotal:"):
                    memory["total"] = int(line.split()[1])

                elif line.startswith("MemAvailable:"):
                    memory["available"] = int(line.split()[1])

    except Exception:
        pass

    total = memory["total"]
    available = memory["available"]

    used = total - available

    if total > 0:

        usage_percent = round(
            (used / total) * 100,
            2
        )

    else:

        usage_percent = 0

    return {
        "memory_total_kb": total,
        "memory_available_kb": available,
        "memory_used_kb": used,
        "memory_usage_percent": usage_percent,
        "cpu_usage_percent": read_system_cpu(),
        "process": read_process_info(pid),
        "uptime_seconds":
            round(time.time() - SERVER_START_TIME, 1)
    }


# ---------------------------------------------------------
# AIPage prediction
#
# This is the GUI/runtime demonstration layer.
# The actual replacement algorithm remains in C.
# ---------------------------------------------------------

def predict_next_page():

    if len(ACCESS_HISTORY) < 5:
        return {
            "predicted_page": None,
            "confidence": 0.0,
            "reason": "Waiting for sufficient page history"
        }

    recent = list(ACCESS_HISTORY)
    current = recent[-1]

    # ---------------------------------------------------------
    # METHOD 1: Repeated transition prediction
    # ---------------------------------------------------------

    transitions = {}

    for i in range(len(recent) - 1):

        source = recent[i]
        target = recent[i + 1]

        if source not in transitions:
            transitions[source] = {}

        transitions[source][target] = (
            transitions[source].get(target, 0) + 1
        )

    candidates = transitions.get(current, {})

    if candidates:

        predicted = max(
            candidates,
            key=candidates.get
        )

        total = sum(candidates.values())

        confidence = round(
            candidates[predicted] / total,
            3
        )

        return {
            "predicted_page": predicted,
            "confidence": confidence,
            "reason": "Repeated page-transition pattern"
        }

    # ---------------------------------------------------------
    # METHOD 2: Recent frequency / locality prediction
    # ---------------------------------------------------------

    frequency = {}

    for page in recent:

        if page != current:
            frequency[page] = frequency.get(page, 0) + 1

    if frequency:

        predicted = max(
            frequency,
            key=frequency.get
        )

        confidence = round(
            frequency[predicted] / len(recent),
            3
        )

        return {
            "predicted_page": predicted,
            "confidence": confidence,
            "reason": "Recent page-frequency locality"
        }

    return {
        "predicted_page": None,
        "confidence": 0.0,
        "reason": "No prediction available"
    }


# ---------------------------------------------------------
# REAL LINUX RESIDENT PAGE MONITOR
# ---------------------------------------------------------

PAGE_SIZE = os.sysconf("SC_PAGE_SIZE")


def read_resident_pages(pid=None, max_pages=3000):

    if pid is None:
        pid = os.getpid()

    maps_file = f"/proc/{pid}/maps"
    pagemap_file = f"/proc/{pid}/pagemap"

    resident_pages = []
    mappings = 0

    try:

        with open(maps_file, "r") as maps:
            regions = maps.readlines()

        with open(pagemap_file, "rb") as pagemap:

            for region in regions:

                parts = region.split()

                if len(parts) < 2:
                    continue

                address_range = parts[0]
                permissions = parts[1]

                if "r" not in permissions:
                    continue

                start_hex, end_hex = address_range.split("-")

                start = int(start_hex, 16)
                end = int(end_hex, 16)

                mappings += 1

                start_page = start // PAGE_SIZE
                end_page = end // PAGE_SIZE

                if end_page - start_page > 2048:
                    end_page = start_page + 2048

                for virtual_page in range(
                    start_page,
                    end_page
                ):

                    if len(resident_pages) >= max_pages:
                        break

                    pagemap.seek(virtual_page * 8)

                    entry = pagemap.read(8)

                    if len(entry) != 8:
                        continue

                    value = int.from_bytes(
                        entry,
                        byteorder="little"
                    )

                    present = (
                        value & (1 << 63)
                    ) != 0

                    if present:

                        virtual_address = (
                            virtual_page * PAGE_SIZE
                        )

                        resident_pages.append({
                            "virtual_page": virtual_page,
                            "address":
                                f"0x{virtual_address:x}",
                            "page_size": PAGE_SIZE,
                            "present": True
                        })

                if len(resident_pages) >= max_pages:
                    break

    except PermissionError:

        return {
            "success": False,
            "error":
                "Permission denied reading /proc/pagemap",
            "pid": pid,
            "page_size": PAGE_SIZE,
            "resident_pages": [],
            "count": 0,
            "mappings": mappings
        }

    except Exception as error:

        return {
            "success": False,
            "error": str(error),
            "pid": pid,
            "page_size": PAGE_SIZE,
            "resident_pages": [],
            "count": 0,
            "mappings": mappings
        }

    return {
        "success": True,
        "pid": pid,
        "page_size": PAGE_SIZE,
        "resident_pages": resident_pages,
        "count": len(resident_pages),
        "mappings": mappings,
        "timestamp":
            time.strftime("%H:%M:%S")
    }


# ---------------------------------------------------------
# Routes
# ---------------------------------------------------------

@app.route("/")
def index():

    results = load_results()

    return render_template(
        "index.html",
        results=results
    )



@app.route("/api/pages")
def api_pages():

    pid = request.args.get(
        "pid",
        default=MONITOR_PID,
        type=int
    )

    return jsonify(
        read_resident_pages(pid)
    )


@app.route("/api/results")
def api_results():

    return jsonify(load_results())


@app.route("/api/system")
def api_system():

    pid = request.args.get(
        "pid",
        default=MONITOR_PID,
        type=int
    )

    return jsonify(
        read_system_info(pid)
    )


# ---------------------------------------------------------
# Live process scheduling telemetry
# ---------------------------------------------------------

def read_live_processes():

    processes = []

    try:

        now = time.time()

        for name in os.listdir("/proc"):

            if not name.isdigit():
                continue

            pid = int(name)

            try:

                stat_path = f"/proc/{pid}/stat"
                status_path = f"/proc/{pid}/status"

                with open(stat_path, "r") as f:
                    stat = f.read()

                # Process name can contain spaces.
                close = stat.rfind(")")
                if close == -1:
                    continue

                after = stat[close + 2:].split()

                if len(after) < 20:
                    continue

                state = after[0]
                utime = int(after[11])
                stime = int(after[12])
                start_ticks = int(after[19])

                clock_ticks = os.sysconf(
                    os.sysconf_names["SC_CLK_TCK"]
                )

                cpu_time = (
                    utime + stime
                ) / clock_ticks

                uptime = 0.0

                try:
                    with open("/proc/uptime", "r") as f:
                        uptime = float(
                            f.read().split()[0]
                        )
                except Exception:
                    pass

                process_age = max(
                    0.0,
                    uptime - (
                        start_ticks / clock_ticks
                    )
                )

                process_name = stat[
                    stat.find("(") + 1:close
                ]

                # Ignore kernel threads where possible.
                if process_name.startswith("kworker"):
                    continue

                processes.append({
                    "pid": pid,
                    "process": process_name,
                    "arrival": round(process_age, 1),
                    "burst": round(cpu_time, 2),
                    "state": {
                        "R": "RUNNING",
                        "S": "SLEEPING",
                        "D": "WAITING",
                        "T": "STOPPED",
                        "Z": "ZOMBIE",
                        "I": "IDLE"
                    }.get(state, state),
                    "cpu_time": round(cpu_time, 2)
                })

            except (
                FileNotFoundError,
                PermissionError,
                ProcessLookupError,
                ValueError,
                IndexError
            ):
                continue

    except Exception as error:

        print(
            "Process telemetry error:",
            error
        )

    # Most CPU-active processes first.
    processes.sort(
        key=lambda x: x["cpu_time"],
        reverse=True
    )

    return processes[:12]


def database_sample_count():
    """Return the number of persisted Linux process telemetry samples."""
    db_path = os.path.join(BASE_DIR, "aipage.db")
    try:
        with sqlite3.connect(db_path, timeout=1) as conn:
            row = conn.execute("SELECT COUNT(*) FROM process_samples").fetchone()
            return int(row[0] or 0)
    except (sqlite3.Error, OSError):
        return 0


@app.route("/api/database")
def api_database():
    return jsonify({
        "online": os.path.exists(os.path.join(BASE_DIR, "aipage.db")),
        "samples": database_sample_count(),
        "table": "process_samples"
    })


@app.route("/api/live/status")
def api_live_status():
    running = live_pipeline_running()
    return jsonify({"running": running, "controller_pid": LIVE_PIPELINE.pid if running else None})


@app.route("/api/live/start", methods=["POST"])
def api_live_start():
    global LIVE_PIPELINE
    if live_pipeline_running():
        return jsonify({"success": True, "running": True, "controller_pid": LIVE_PIPELINE.pid})
    log = open(LIVE_PIPELINE_LOG, "a")
    try:
        LIVE_PIPELINE = subprocess.Popen(
            ["bash", "-lc", "./bin/real_memory_workload | ./bin/realtime_ai_FINAL"],
            cwd=BASE_DIR, stdout=log, stderr=subprocess.STDOUT,
            start_new_session=True
        )
        set_live_state_running(True)
        return jsonify({"success": True, "running": True, "controller_pid": LIVE_PIPELINE.pid})
    except OSError as e:
        log.close()
        LIVE_PIPELINE = None
        return jsonify({"success": False, "error": str(e)}), 500


@app.route("/api/live/stop", methods=["POST"])
def api_live_stop():
    global LIVE_PIPELINE
    if live_pipeline_running():
        try:
            os.killpg(LIVE_PIPELINE.pid, signal.SIGTERM)
            LIVE_PIPELINE.wait(timeout=3)
        except (ProcessLookupError, subprocess.TimeoutExpired, OSError):
            try:
                os.killpg(LIVE_PIPELINE.pid, signal.SIGKILL)
            except (ProcessLookupError, OSError):
                pass
    LIVE_PIPELINE = None
    set_live_state_running(False)
    return jsonify({"success": True, "running": False})


@app.route("/api/processes")
def api_processes():

    return jsonify({
        "success": True,
        "algorithm": "ROUND ROBIN",
        "quantum": 2,
        "cpu_count": os.cpu_count() or 1,
        "processes": read_live_processes()
    })



# ---------------------------------------------------------
# Client authorization
# ---------------------------------------------------------

@app.route("/api/connect", methods=["POST"])
def connect_client():

    data = request.get_json(silent=True) or {}

    client_name = data.get(
        "client_name",
        "Unknown Client"
    )

    client_id = str(uuid.uuid4())

    CLIENTS[client_id] = {
        "client_name": client_name,
        "connected_at": time.strftime("%H:%M:%S"),
        "authorized": False,
        "last_seen": time.strftime("%H:%M:%S")
    }

    return jsonify({
        "success": True,
        "client_id": client_id,
        "authorized": False,
        "message":
            "Monitoring permission is required."
    })


@app.route("/api/authorize", methods=["POST"])
def authorize_client():

    data = request.get_json(silent=True) or {}

    client_id = data.get("client_id")
    permission = bool(data.get("permission"))

    if client_id not in CLIENTS:

        return jsonify({
            "success": False,
            "message": "Unknown client."
        }), 404

    CLIENTS[client_id]["authorized"] = permission
    CLIENTS[client_id]["last_seen"] = time.strftime(
        "%H:%M:%S"
    )

    return jsonify({
        "success": True,
        "authorized": permission
    })


@app.route("/api/clients")
def clients():

    return jsonify({
        "count": len(CLIENTS),
        "clients": CLIENTS
    })


# ---------------------------------------------------------
# Page access submission
# ---------------------------------------------------------

@app.route("/api/access", methods=["POST"])
def record_access():

    data = request.get_json(silent=True) or {}

    client_id = data.get("client_id")
    page = data.get("page")

    if not client_id or client_id not in CLIENTS:

        return jsonify({
            "success": False,
            "message": "Unknown client."
        }), 404

    if not CLIENTS[client_id]["authorized"]:

        return jsonify({
            "success": False,
            "message": "Client has not authorized monitoring."
        }), 403

    if page is None:

        return jsonify({
            "success": False,
            "message": "Page is required."
        }), 400

    try:

        page = int(page)

    except ValueError:

        return jsonify({
            "success": False,
            "message": "Page must be an integer."
        }), 400

    ACCESS_HISTORY.append(page)

    CURRENT_STATE["current_page"] = page

    prediction = predict_next_page()

    CURRENT_STATE["predicted_page"] = (
        prediction["predicted_page"]
    )

    CURRENT_STATE["confidence"] = (
        prediction["confidence"]
    )

    CLIENTS[client_id]["last_seen"] = time.strftime(
        "%H:%M:%S"
    )

    return jsonify({
        "success": True,
        "current_page": page,
        "prediction": prediction
    })


# ---------------------------------------------------------
# Live AIPage state
# ---------------------------------------------------------

@app.route("/api/live_processes")
def api_live_processes():

    processes = []

    try:
        for entry in Path("/proc").iterdir():

            if not entry.name.isdigit():
                continue

            pid = int(entry.name)

            try:
                stat = Path(f"/proc/{pid}/stat").read_text()
                status = Path(f"/proc/{pid}/status").read_text()

                # /proc/<pid>/stat: command is field 2, state field 3
                close = stat.rfind(")")
                if close < 0:
                    continue

                rest = stat[close + 2:].split()

                state = rest[0] if rest else "?"
                utime = int(rest[11]) if len(rest) > 11 else 0
                stime = int(rest[12]) if len(rest) > 12 else 0
                start_ticks = int(rest[19]) if len(rest) > 19 else 0

                hz = os.sysconf(os.sysconf_names["SC_CLK_TCK"])
                cpu_seconds = (utime + stime) / hz

                uptime = float(Path("/proc/uptime").read_text().split()[0])
                start_seconds_ago = max(0, uptime - (start_ticks / hz))

                name = entry.name
                for line in status.splitlines():
                    if line.startswith("Name:"):
                        name = line.split(":", 1)[1].strip()
                        break

                state_map = {
                    "R": "RUNNING",
                    "S": "SLEEPING",
                    "D": "WAITING",
                    "T": "STOPPED",
                    "Z": "ZOMBIE",
                    "I": "IDLE"
                }

                processes.append({
                    "pid": pid,
                    "name": name,
                    "state": state_map.get(state, state),
                    "arrival": round(start_seconds_ago, 1),
                    "burst": round(cpu_seconds, 2),
                    "cpu_time": round(cpu_seconds, 2)
                })

            except (FileNotFoundError, PermissionError, ProcessLookupError,
                    ValueError, IndexError):
                continue

    except Exception as e:
        return jsonify({
            "success": False,
            "error": str(e),
            "processes": []
        }), 500

    processes.sort(key=lambda x: x["pid"])

    # Keep dashboard readable while still representing the live system.
    visible = processes[:12]

    return jsonify({
        "success": True,
        "count": len(processes),
        "processes": visible,
        "total_system_processes": len(processes)
    })


@app.route("/api/state")
def api_state():

    live = load_live_state()

    return jsonify({

        "running":
            live["running"],

        "access_count":
            live["access_count"],

        "current_page":
            live["current_page"],

        "predicted_page":
            live["predicted_page"],

        "confidence":
            live["confidence"],

        "prediction_reason":
            live["reason"],

        "recent_accesses":
            live["recent_pages"],

        "resident_frames":
            live["resident_frames"],

        "hit_fault":
            live["hit_fault"],

        "victim_page":
            live["victim_page"],

        "decision":
            live["decision"],

        "reason":
            live["reason"],

        "page_hits":
            live["page_hits"],

        "page_faults":
            live["page_faults"],

        "replacements":
            live["replacements"],

        "prediction_count":
            live["prediction_count"],

        "prediction_correct":
            live["prediction_correct"],

        "hit_rate":
            live["hit_rate"],

        "fault_rate":
            live["fault_rate"],

        "prediction_accuracy":
            live["prediction_accuracy"],

        "last_prediction_result":
            live.get("last_prediction_result")
    })


# ---------------------------------------------------------
# Server
# ---------------------------------------------------------

if __name__ == "__main__":

    print()
    print("=" * 60)
    print("AIPage v2.1")
    print("Intelligent Linux Page Replacement Framework")
    print("=" * 60)
    print("Server starting on port 5000...")
    print(f"Monitoring PID: {MONITOR_PID}")
    print()

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=False
    )
