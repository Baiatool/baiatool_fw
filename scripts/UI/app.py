import threading
from datetime import datetime, timedelta, timezone

from flask import Flask, abort, jsonify, redirect, render_template, request, url_for

app = Flask(__name__)

MAX_ID_LENGTH = 16
EXTEND_MINUTES = 60
FIRST_USE_TIMEOUT_MINUTES = 120  # CONFIG_SCHEDULE_FIRST_USE_TIMEOUT_MINUTES

BRT = timezone(timedelta(hours=-3))


class ScheduleStore:
    def __init__(self):
        self._lock = threading.Lock()
        self._schedule = None  # baiatool_schedule_state + status field
        self._events = []      # list of {timestamp, event, user_id}

    @property
    def schedule(self):
        return self._schedule

    @property
    def events(self):
        return self._events

    def register(self, user_id: str, start_dt: datetime, end_dt: datetime) -> bool:
        """Returns True on success, False if a pending/active slot already exists."""
        with self._lock:
            if self._schedule and self._schedule["status"] in ("pending", "active"):
                return False
            self._schedule = {
                "user_id": user_id,
                "last_cmd": "end_use",
                "start_time": start_dt,
                "end_time": end_dt,
                "status": "pending",
            }
            self._log("registered", user_id)
            return True

    def confirm(self, user_id: str) -> None:
        self._check(user_id)
        self._schedule["status"] = "active"
        self._schedule["last_cmd"] = "first_use"
        self._log("confirmed", user_id)

    def cancel(self, user_id: str) -> None:
        self._check(user_id)
        self._schedule["status"] = "cancelled"
        self._schedule["last_cmd"] = "end_use"
        self._log("cancelled (no-show)", user_id)

    def end(self, user_id: str) -> None:
        self._check(user_id)
        self._schedule["status"] = "ended"
        self._schedule["last_cmd"] = "end_use"
        self._log("ended (checkout)", user_id)

    def extend(self, user_id: str) -> None:
        self._check(user_id)
        self._schedule["end_time"] += timedelta(minutes=EXTEND_MINUTES)
        self._schedule["last_cmd"] = "extend_time"
        self._log("extended", user_id)

    def to_firmware_json(self) -> dict:
        """baiatool_schedule_state shape — consumed by the firmware."""
        s = self._schedule
        return {
            "user_id": s["user_id"],
            "last_cmd": s["last_cmd"],
            "start_time": int(s["start_time"].timestamp()),
            "end_time": int(s["end_time"].timestamp()),
        }

    def to_status_json(self) -> dict:
        """UI-friendly shape — consumed by the JS polling loop."""
        s = self._schedule
        return {
            "schedule": None if s is None else {
                "user_id": s["user_id"],
                "last_cmd": s["last_cmd"],
                "start_time": s["start_time"].strftime("%H:%M"),
                "end_time": s["end_time"].strftime("%H:%M"),
                "status": s["status"],
            },
            "events": self._events[:10],
        }

    def _check(self, user_id: str) -> None:
        if self._schedule is None:
            abort(404)
        if self._schedule["user_id"] != user_id:
            abort(409)

    def _log(self, event: str, user_id: str) -> None:
        self._events.insert(0, {
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "event": event,
            "user_id": user_id,
        })
        del self._events[20:]


store = ScheduleStore()

@app.get("/")
def index():
    return render_template("index.html", schedule=store.schedule, events=store.events[:10])


@app.post("/ui/schedule")
def schedule_register():
    user_id = request.form.get("user_id", "").strip()[:MAX_ID_LENGTH]
    start_str = request.form.get("start_time", "")

    if not user_id or not start_str:
        return render_template(
            "index.html",
            schedule=store.schedule,
            events=store.events[:10],
            error="All fields are required.",
        ), 400

    today = datetime.now(BRT).date()
    start_dt = datetime.combine(today, datetime.strptime(start_str, "%H:%M").time(), tzinfo=BRT)
    end_dt   = start_dt + timedelta(minutes=FIRST_USE_TIMEOUT_MINUTES)

    if not store.register(user_id, start_dt, end_dt):
        return render_template(
            "index.html",
            schedule=store.schedule,
            events=store.events[:10],
            error="A schedule is already active or pending.",
        ), 409

    return redirect(url_for("index"))

@app.get("/api/status")
def api_status():
    return jsonify(store.to_status_json())


@app.get("/api/schedule")
def api_get_schedule():
    if store.schedule is None or store.schedule["status"] not in ("pending", "active"):
        abort(404)
    return jsonify(store.to_firmware_json())


@app.post("/api/schedule/confirm")
def api_confirm():
    body = request.get_json(silent=True) or {}
    store.confirm(body.get("user_id", ""))
    return jsonify(store.to_firmware_json())


@app.post("/api/schedule/cancel")
def api_cancel():
    body = request.get_json(silent=True) or {}
    store.cancel(body.get("user_id", ""))
    return jsonify(store.to_firmware_json())


@app.post("/api/schedule/end")
def api_end():
    body = request.get_json(silent=True) or {}
    store.end(body.get("user_id", ""))
    return jsonify(store.to_firmware_json())


@app.post("/api/schedule/extend")
def api_extend():
    body = request.get_json(silent=True) or {}
    store.extend(body.get("user_id", ""))
    return jsonify(store.to_firmware_json())


if __name__ == "__main__":
    app.run(debug=True)
