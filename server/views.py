from flask import Blueprint, render_template, request
from forms import LoginForm

views = Blueprint('views', __name__)

@views.route('/')
def home():
    return render_template('home.html', user='John Doe')

@views.route('/login', methods = ['GET','POST'])
def login():
    form = LoginForm()
    return render_template('login.html', form=form)

@views.route('/tags')
def tags():
    return render_template('tags.html')