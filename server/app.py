from os import environ
from flask import Flask
from flask_sqlalchemy import SQLAlchemy
from views import views
from models import User
from flask_login import LoginManager

app = Flask(__name__)
app.register_blueprint(views, url_prefix="/")

app.config['SQLALCHEMY_DATABASE_URI'] = environ.get('DATABASE_URL')
app.config['SECRET_KEY'] = environ.get('SECRET_KEY')

db = SQLAlchemy(app)

login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view="login"

from models import *

if __name__ == '__main__':
    app.run(debug=True, port=80)
    #admin = User('admin, 'password')
    #db.session.add(admin)
    #db.session.commit()
    db.create_all()