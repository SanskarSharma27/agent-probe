import os
import time
from flask import Flask, request, jsonify
from datetime import datetime
import requests

app = Flask(__name__)


class DatabaseClient:
    def __init__(self, connection_string):
        self.conn = connection_string

    def query(self, sql, params=None):
        return []

    def execute(self, sql, params=None):
        pass


class UserService:
    def __init__(self, db):
        self.db = db

    def get_user(self, user_id):
        return self.db.query("SELECT * FROM users WHERE id = ?", [user_id])

    def create_user(self, name, email):
        self.db.execute("INSERT INTO users (name, email) VALUES (?, ?)", [name, email])
        return {"name": name, "email": email}

    def delete_user(self, user_id):
        self.db.execute("DELETE FROM users WHERE id = ?", [user_id])


db = DatabaseClient(os.getenv("DATABASE_URL"))
user_service = UserService(db)


@app.route("/users", methods=["GET"])
def list_users():
    users = user_service.get_user("all")
    return jsonify(users)


@app.route("/users", methods=["POST"])
def create_user():
    data = request.get_json()
    user = user_service.create_user(data["name"], data["email"])
    return jsonify(user), 201


@app.route("/external/weather")
def get_weather():
    response = requests.get("https://api.weather.com/v1/current")
    return jsonify(response.json())


def sync_with_retry(url, max_retries=3):
    for attempt in range(max_retries):
        try:
            response = requests.post(url, json={"sync": True})
            if response.status_code == 200:
                return response.json()
        except Exception:
            pass
        time.sleep(2 ** attempt)
    return None
