from os import environ
from flask import Flask
from flask_sqlalchemy import SQLAlchemy
from views import views

app = Flask(__name__)
app.register_blueprint(views, url_prefix="/")

app.config['SQLALCHEMY_DATABASE_URI'] = environ.get('DATABASE_URL')
app.config['SECRET_KEY'] = environ.get('SECRET_KEY')

db = SQLAlchemy(app)

from models import *

if __name__ == '__main__':
    app.run(debug=True, port=80)
    db.create_all()