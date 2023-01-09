from app import db
from flask_login import UserMixin

# Tabelas do banco de dados
class User(db.Model, UserMixin):
    id = db.Column('user_id', db.Integer, primary_key=True)
    username = db.Column(db.String(30), unique=True, nullable=False)
    password = db.Column(db.String(80), nullable=False)

    def __init__(self, username, password):
        self.username = username
        self.password = password

class Tag(db.Model):
    id = db.Column('tag_id', db.Integer, primary_key=True)
    tag = db.Column(db.String(8), unique=True, nullable=False)
    name = db.Column(db.String(30), nullable=False)
    email = db.Column(db.String(30), nullable=False)

    def __init__(self, tag, name, email):
        self.tag = tag
        self.name = name
        self.email = email