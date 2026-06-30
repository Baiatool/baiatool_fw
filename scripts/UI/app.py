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
        self._schedules = []  # sorted by start_time
        self._events = []

    @property
    def events(self):
        return self._events

    def register(self, user_id: str, start_dt: datetime, end_dt: datetime) -> bool:
        """Returns True on success, False if the slot overlaps an existing pending/active one."""
        with self._lock:
            for s in self._schedules:
                if s["status"] not in ("pending", "active"):
                    continue
                if start_dt < s["end_time"] and end_dt > s["start_time"]:
                    return False
            self._schedules.append({
                "user_id": user_id,
                "last_cmd": "end_use",
                "start_time": start_dt,
                "end_time": end_dt,
                "status": "pending",
            })
            self._schedules.sort(key=lambda s: s["start_time"])
            self._log("registered", user_id)
            return True

    def confirm(self, user_id: str) -> dict:
        s = self._find(user_id)
        s["status"] = "active"
        s["last_cmd"] = "first_use"
        self._log("confirmed", user_id)
        return s

    def cancel(self, user_id: str) -> dict:
        s = self._find(user_id)
        s["status"] = "cancelled"
        s["last_cmd"] = "end_use"
        self._log("cancelled (no-show)", user_id)
        return s

    def end(self, user_id: str) -> dict:
        s = self._find(user_id)
        s["status"] = "ended"
        s["last_cmd"] = "end_use"
        self._log("ended (checkout)", user_id)
        return s

    def extend(self, user_id: str) -> dict:
        s = self._find(user_id)
        s["end_time"] += timedelta(minutes=EXTEND_MINUTES)
        s["last_cmd"] = "extend_time"
        self._log("extended", user_id)
        return s

    def to_firmware_json(self, s: dict) -> dict:
        return {
            "user_id": s["user_id"],
            "last_cmd": s["last_cmd"],
            "start_time": int(s["start_time"].timestamp()),
            "end_time": int(s["end_time"].timestamp()),
        }

    def to_status_json(self) -> dict:
        current = self._active() or self._next_pending()
        queue = [s for s in self._schedules if s["status"] == "pending"]
        return {
            "schedule": None if current is None else {
                "user_id": current["user_id"],
                "last_cmd": current["last_cmd"],
                "start_time": current["start_time"].strftime("%H:%M"),
                "end_time": current["end_time"].strftime("%H:%M"),
                "status": current["status"],
            },
            "queue": [
                {
                    "user_id": s["user_id"],
                    "start_time": s["start_time"].strftime("%H:%M"),
                    "end_time": s["end_time"].strftime("%H:%M"),
                }
                for s in queue
            ],
            "events": self._events[:10],
        }

    def _active(self):
        for s in self._schedules:
            if s["status"] == "active":
                return s
        return None

    def _next_pending(self):
        for s in self._schedules:
            if s["status"] == "pending":
                return s  # list is sorted by start_time
        return None

    def _find(self, user_id: str) -> dict:
        for s in self._schedules:
            if s["user_id"] == user_id and s["status"] in ("pending", "active"):
                return s
        abort(404)

    def _log(self, event: str, user_id: str) -> None:
        self._events.insert(0, {
            "timestamp": datetime.now(BRT).strftime("%Y-%m-%d %H:%M:%S"),
            "event": event,
            "user_id": user_id,
        })
        del self._events[20:]


store = ScheduleStore()


@app.get("/")
def index():
    status = store.to_status_json()
    return render_template("index.html",
                           schedule=status["schedule"],
                           queue=status["queue"],
                           events=status["events"])


@app.post("/ui/schedule")
def schedule_register():
    user_id = request.form.get("user_id", "").strip()[:MAX_ID_LENGTH]
    start_str = request.form.get("start_time", "")

    if not user_id or not start_str:
        status = store.to_status_json()
        return render_template(
            "index.html",
            schedule=status["schedule"],
            queue=status["queue"],
            events=status["events"],
            error="All fields are required.",
        ), 400

    today = datetime.now(BRT).date()
    start_dt = datetime.combine(today, datetime.strptime(start_str, "%H:%M").time(), tzinfo=BRT)
    end_dt = start_dt + timedelta(minutes=FIRST_USE_TIMEOUT_MINUTES)

    if not store.register(user_id, start_dt, end_dt):
        status = store.to_status_json()
        return render_template(
            "index.html",
            schedule=status["schedule"],
            queue=status["queue"],
            events=status["events"],
            error="Time slot overlaps an existing pending or active schedule.",
        ), 409

    return redirect(url_for("index"))


@app.get("/api/status")
def api_status():
    return jsonify(store.to_status_json())


@app.get("/api/schedule")
def api_get_schedule():
    s = store._next_pending()
    if s is None:
        abort(404)
    return jsonify(store.to_firmware_json(s))


@app.post("/api/schedule/confirm")
def api_confirm():
    body = request.get_json(silent=True) or {}
    s = store.confirm(body.get("user_id", ""))
    return jsonify(store.to_firmware_json(s))


@app.post("/api/schedule/cancel")
def api_cancel():
    body = request.get_json(silent=True) or {}
    s = store.cancel(body.get("user_id", ""))
    return jsonify(store.to_firmware_json(s))


@app.post("/api/schedule/end")
def api_end():
    body = request.get_json(silent=True) or {}
    s = store.end(body.get("user_id", ""))
    return jsonify(store.to_firmware_json(s))


@app.post("/api/schedule/extend")
def api_extend():
    body = request.get_json(silent=True) or {}
    s = store.extend(body.get("user_id", ""))
    return jsonify(store.to_firmware_json(s))


if __name__ == "__main__":
    app.run(host="0.0.0.0", debug=True)
