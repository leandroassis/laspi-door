from flask import Blueprint, render_template, request, url_for, redirect
from flask_login import login_user, login_required, logout_user, current_user
from forms import LoginForm
from flask_bcrypt import Bcrypt
from app import login_manager
from models import User
from os import environ

views = Blueprint('views', __name__)

@login_manager.user_loader
def load_user(id):
    return User.query.get(int(id))

@views.route('/')
def home():
    # página inicial -> direciona pro login caso não esteja autenticado
    return redirect(url_for('dashboard'))

@views.route('/login', methods = ['GET','POST'])
def login():
    # formulário de login
    form = LoginForm()

    if form.validate_on_submit():
        hashed = Bcrypt.generate_password_hash(form.password.data)
        user = User.query.filter_by(username=form.username.data).first()
        if user:
            if Bcrypt.check_password_hash(user.password, form.password.data):
                login_user(user)
                return redirect(url_for('dashboard'))
    return hashed #render_template('login.html', form=form)

@views.route('/logout', methods = ['GET', 'POST'])
@login_required
def logout():
    logout_user()
    return redirect(url_for('login'))

@views.route('/dashboard')
@login_required
def dashboard():
    return render_template('dashboard.html')

@views.route(f"/{environ.get('LISTAR_TAGS')}")
def tags():
    # retorna as tags do banco de dados pro arduino
    return render_template('tags.html')

@views.route(f"/{environ.get('ENVIAR_ULTIMA_TAG')}")
def tag():
    # recebe a última tag lida pela porta
    args = request.args
    tag_uid = args.get('uid')
    return "Tag UID: " + tag_uid

@views.route('/cadastrar', methods = ['GET', 'POST'])
@login_required
def cadastrar():
    return render_template('cadastro.html')

@views.route('/remover', methods = ['GET', 'POST'])
@login_required
def remover():
    return render_template('remover.html')