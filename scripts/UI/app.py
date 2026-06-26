import threading
from datetime import datetime, timedelta, timezone

from flask import Flask, abort, jsonify, redirect, render_template, request, url_for

app = Flask(__name__)

MAX_ID_LENGTH = 16
EXTEND_MINUTES = 60

schedule = None  # baiatool_schedule_state + status field
events = []      # list of {timestamp, event, user_id}
_lock = threading.Lock()


def _log(event: str, user_id: str) -> None:
    events.insert(0, {
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "event": event,
        "user_id": user_id,
    })
    del events[20:]


def _schedule_json() -> dict:
    return {
        "user_id": schedule["user_id"],
        "last_cmd": schedule["last_cmd"],
        "start_time": int(schedule["start_time"].timestamp()),
        "end_time": int(schedule["end_time"].timestamp()),
    }


# ── Browser routes ─────────────────────────────────────────────────────────────

@app.get("/")
def index():
    return render_template("index.html", schedule=schedule, events=events[:10])


@app.post("/ui/schedule")
def ui_register():
    global schedule

    user_id = request.form.get("user_id", "").strip()[:MAX_ID_LENGTH]
    start_str = request.form.get("start_time", "")
    end_str = request.form.get("end_time", "")

    if not user_id or not start_str or not end_str:
        return render_template(
            "index.html",
            schedule=schedule,
            events=events[:10],
            error="All fields are required.",
        ), 400

    start_dt = datetime.fromisoformat(start_str).astimezone(timezone.utc)
    end_dt = datetime.fromisoformat(end_str).astimezone(timezone.utc)

    if end_dt <= start_dt:
        return render_template(
            "index.html",
            schedule=schedule,
            events=events[:10],
            error="End time must be after start time.",
        ), 400

    with _lock:
        if schedule and schedule["status"] in ("pending", "active"):
            return render_template(
                "index.html",
                schedule=schedule,
                events=events[:10],
                error="A schedule is already active or pending.",
            ), 409

        schedule = {
            "user_id": user_id,
            "last_cmd": "end_use",
            "start_time": start_dt,
            "end_time": end_dt,
            "status": "pending",
        }
        _log("registered", user_id)

    return redirect(url_for("index"))


@app.get("/api/status")
def api_status():
    """Polling endpoint for the JS live-update — returns UI-friendly state."""
    s = None
    if schedule:
        s = {
            "user_id": schedule["user_id"],
            "last_cmd": schedule["last_cmd"],
            "start_time": schedule["start_time"].strftime("%Y-%m-%d %H:%M"),
            "end_time": schedule["end_time"].strftime("%Y-%m-%d %H:%M"),
            "status": schedule["status"],
        }
    return jsonify({"schedule": s, "events": events[:10]})


# ── Firmware API routes ─────────────────────────────────────────────────────────

@app.get("/api/schedule")
def api_get_schedule():
    if schedule is None or schedule["status"] not in ("pending", "active"):
        abort(404)
    return jsonify(_schedule_json())


@app.post("/api/schedule/confirm")
def api_confirm():
    global schedule

    body = request.get_json(silent=True) or {}
    user_id = body.get("user_id", "")

    if schedule is None:
        abort(404)
    if schedule["user_id"] != user_id:
        abort(409)

    schedule["status"] = "active"
    schedule["last_cmd"] = "first_use"
    _log("confirmed", user_id)
    return jsonify(_schedule_json())


@app.post("/api/schedule/cancel")
def api_cancel():
    global schedule

    body = request.get_json(silent=True) or {}
    user_id = body.get("user_id", "")

    if schedule is None:
        abort(404)
    if schedule["user_id"] != user_id:
        abort(409)

    schedule["status"] = "cancelled"
    schedule["last_cmd"] = "end_use"
    _log("cancelled (no-show)", user_id)
    return jsonify(_schedule_json())


@app.post("/api/schedule/end")
def api_end():
    global schedule

    body = request.get_json(silent=True) or {}
    user_id = body.get("user_id", "")

    if schedule is None:
        abort(404)
    if schedule["user_id"] != user_id:
        abort(409)

    schedule["status"] = "ended"
    schedule["last_cmd"] = "end_use"
    _log("ended (checkout)", user_id)
    return jsonify(_schedule_json())


@app.post("/api/schedule/extend")
def api_extend():
    global schedule

    body = request.get_json(silent=True) or {}
    user_id = body.get("user_id", "")

    if schedule is None:
        abort(404)
    if schedule["user_id"] != user_id:
        abort(409)

    schedule["end_time"] += timedelta(minutes=EXTEND_MINUTES)
    schedule["last_cmd"] = "extend_time"
    _log("extended", user_id)
    return jsonify(_schedule_json())


if __name__ == "__main__":
    app.run(debug=True)
