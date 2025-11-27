import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from PIL import Image, ImageTk
import io
import random
import re
import smtplib
from email.mime.text import MIMEText
import mysql.connector
from datetime import datetime
import pytz
import cv2


try:
    RESAMPLE_MODE = Image.Resampling.LANCZOS  # type: ignore
except AttributeError:
    try:
        RESAMPLE_MODE = Image.LANCZOS  # type: ignore
    except AttributeError:
        RESAMPLE_MODE = Image.BICUBIC  # type: ignore


DB_CONFIG = {
    'host': 'localhost',
    'user': 'root',
    'password': '',
    'database': 'cerradura_db'
}


EMAIL_REMITENTE = "jacha.tech19@gmail.com"
EMAIL_PASSWORD_APP = "hdmg jsol tyvv tcoy"


def hora_actual_lapaz():
    tz = pytz.timezone('America/La_Paz')
    return datetime.now(tz).strftime("%Y-%m-%d %H:%M:%S")


def saludo_por_hora_lapaz():
    tz = pytz.timezone('America/La_Paz')
    hora = datetime.now(tz).hour
    if 6 <= hora < 12:
        return "Buenos días,"
    elif 12 <= hora < 19:
        return "Buenas tardes,"
    else:
        return "Buenas noches,"


def enviar_correo_obtencion_contraseña(nombre_completo, celular, correo_usuario, contraseña_generada):
    remitente = EMAIL_REMITENTE
    contraseña_email = EMAIL_PASSWORD_APP
    asunto = "Obtención de contraseña - JACH'A TECH"
    hora_envio = hora_actual_lapaz()
    gerente_nombre = "Victor Hugo Apaza Portanda"
    gerente_celular = "62340488"
    saludo = saludo_por_hora_lapaz()
    cuerpo = f"""
---------- Mensaje reenviado ---------   
De: {gerente_nombre} | Gerente de Desarrollo Tecnológico JACH’A TECH
Para: {nombre_completo} 
Correo: {correo_usuario}  
Fecha: {hora_envio}   
Ref.: Obtención de contraseña - JACH'A TECH   
Correo de la empresa: {remitente}
Cel(591): {gerente_celular}   
---
Calle 21 de Calacoto esq. Pasaje Jose Aguirre | La Paz - Bolivia
---
Asunto: Registro de usuario y contraseña

{saludo} de parte del Sistema de Registro de JACH’A TECH, te informamos que:
Se ha registrado correctamente tu usuario en el sistema.

Tu contraseña es: {contraseña_generada}

Por favor guarda esta información para futuros accesos.

Saludos cordiales,   
{gerente_nombre}   
JACH’A TECH
"""
    mensaje = MIMEText(cuerpo)
    mensaje['Subject'] = asunto
    mensaje['From'] = remitente
    mensaje['To'] = correo_usuario


    try:
        with smtplib.SMTP_SSL('smtp.gmail.com', 465) as server:
            server.login(remitente, contraseña_email)
            server.sendmail(remitente, correo_usuario, mensaje.as_string())
    except Exception as e:
        print("Error en envío de correo:", e)


def generar_contraseña_unica(cursor):
    while True:
        pwd = ''.join(random.choices('0123456789', k=7))
        cursor.execute("SELECT COUNT(*) FROM Personas WHERE contraseña=%s", (pwd,))
        if cursor.fetchone()[0] == 0:
            return pwd


def mostrar_foto_en_pantalla_completa(foto_bytes):
    ventana = tk.Toplevel()
    ventana.attributes('-fullscreen', True)
    ventana.configure(bg='black')
    img = Image.open(io.BytesIO(foto_bytes))
    ancho_ventana = ventana.winfo_screenwidth()
    alto_ventana = ventana.winfo_screenheight()
    img.thumbnail((ancho_ventana, alto_ventana), resample=RESAMPLE_MODE)
    img_tk = ImageTk.PhotoImage(img)
    label = tk.Label(ventana, image=img_tk, bg='black')
    label.image = img_tk  # type: ignore
    label.pack(expand=True)
    btn_cerrar = tk.Button(ventana, text="Cerrar", command=ventana.destroy, font=('Segoe UI', 14), bg='red', fg='white')
    btn_cerrar.pack(pady=10)


def crear_boton_volver(master, command):
    return tk.Button(master, text="Volver", font=("Segoe UI", 13, "bold"), height=2, width=10,
                    bg="#999999", fg="white", command=command)


def crear_boton_ver_foto(master, command):
    return tk.Button(master, text="Ver Foto", command=command, bg="#4a90e2", fg="white",
                    font=("Segoe UI", 12, "bold"), height=2, width=12)


class RegistroFrame(tk.Frame):
    def __init__(self, master, volver):
        super().__init__(master, bg="#f3f3f7")
        self.pack(fill="both", expand=True)
        self.volver = volver
        self.foto_path = None


        tk.Label(self, text="Registrar Nuevo Usuario", font=("Segoe UI", 16, "bold"), bg="#f3f3f7").pack(pady=10)
        form = tk.Frame(self, bg="#f3f3f7")
        form.pack(pady=10)
        campos = ["Nombre", "Apellido", "C.I.", "Celular", "Correo"]
        self.entries = {}
        for i, campo in enumerate(campos):
            tk.Label(form, text=campo + ":", bg="#f3f3f7", font=("Segoe UI", 12)).grid(row=i, column=0, sticky='e', padx=10, pady=5)
            entry = tk.Entry(form, font=("Segoe UI", 11), width=40)
            entry.grid(row=i, column=1, padx=10, pady=5)
            self.entries[campo] = entry


        tk.Label(form, text="Contraseña: (Generada automáticamente)", bg="#f3f3f7", font=("Segoe UI", 12, "italic")).grid(row=len(campos), column=0, columnspan=2, pady=15)


        foto_btns = tk.Frame(self, bg="#f3f3f7")
        foto_btns.pack(fill="x", pady=(12,0))
        tk.Button(foto_btns, text="Seleccionar Foto desde Archivo", font=("Segoe UI", 10, "bold"), height=2, width=25,
                  command=self.cargar_foto).pack(side=tk.LEFT, padx=18, pady=5)
        tk.Button(foto_btns, text="Tomar Foto con Cámara", font=("Segoe UI", 10, "bold"), height=2, width=19,
                  command=self.tomar_foto).pack(side=tk.RIGHT, padx=18, pady=5)


        self.foto_label = tk.Label(self, bg="#f3f3f7")
        self.foto_label.pack(pady=16)


        botones_frame = tk.Frame(self, bg="#f3f3f7")
        botones_frame.pack(fill="x", side=tk.BOTTOM, pady=(0, 10))
        tk.Button(botones_frame, text="Guardar Usuario", font=("Segoe UI", 13, "bold"), height=2, width=14,
                bg="#4a90e2", fg="white", command=self.guardar_usuario).pack(side=tk.LEFT, padx=30, pady=10)
        boton_volver = crear_boton_volver(botones_frame, self.volver)
        boton_volver.pack(side=tk.RIGHT, padx=30, pady=10)


    def cargar_foto(self):
        path = filedialog.askopenfilename(filetypes=[("Imágenes", "*.jpg *.jpeg *.png")])
        if not path:
            return
        self.foto_path = path
        img = Image.open(path)
        img.thumbnail((180, 180), resample=RESAMPLE_MODE)
        img_tk = ImageTk.PhotoImage(img)
        self.foto_label.config(image=img_tk)
        self.foto_label.image = img_tk  # type: ignore


    def tomar_foto(self):
        cap = cv2.VideoCapture(0)
        if not cap.isOpened():
            messagebox.showerror("Error", "No se pudo acceder a la cámara.")
            return
        messagebox.showinfo("Cámara", "Presiona ESPACIO para capturar la foto y ESC para cancelar.")
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            cv2.imshow("Presiona ESPACIO para capturar", frame)
            key = cv2.waitKey(1)
            if key == 27:
                break
            elif key == 32:
                filename = "temp_foto.jpg"
                cv2.imwrite(filename, frame)
                self.foto_path = filename
                break
        cap.release()
        cv2.destroyAllWindows()
        if self.foto_path:
            img = Image.open(self.foto_path)
            img.thumbnail((180, 180), resample=RESAMPLE_MODE)
            img_tk = ImageTk.PhotoImage(img)
            self.foto_label.config(image=img_tk)
            self.foto_label.image = img_tk  # type: ignore


    def guardar_usuario(self):
        datos = {k: e.get().strip() for k, e in self.entries.items()}
        if not all(datos.values()) or not self.foto_path:
            messagebox.showerror("Error", "Complete todos los campos y seleccione una foto.")
            return
        if not datos["C.I."].isdigit():
            messagebox.showerror("Error", "C.I. debe contener solo números.")
            return
        if not re.match(r'^\+?\d+$', datos["Celular"]):
            messagebox.showerror("Error", "Celular debe contener solo números y opcionalmente un '+' al inicio.")
            return
        if not datos["Correo"].endswith("@gmail.com"):
            messagebox.showerror("Error", "Correo debe ser una dirección válida y terminar con '@gmail.com'.")
            return
        with open(self.foto_path, "rb") as f:
            foto_bytes = f.read()
        try:
            conn = mysql.connector.connect(**DB_CONFIG)
            cursor = conn.cursor()
            pwd = generar_contraseña_unica(cursor)
            sql = """INSERT INTO Personas (nombre, apellido, ci, celular, correo, contraseña, ruta_foto, activo)
                     VALUES (%s,%s,%s,%s,%s,%s,%s, 1)"""
            cursor.execute(sql, (datos["Nombre"], datos["Apellido"], datos["C.I."], datos["Celular"], datos["Correo"], pwd, foto_bytes))
            conn.commit()
            cursor.close()
            conn.close()
            enviar_correo_obtencion_contraseña(f'{datos["Nombre"]} {datos["Apellido"]}', datos["Celular"], datos["Correo"], pwd)
            messagebox.showinfo("Éxito", f"Usuario registrado.\nSe envió la contraseña a {datos['Correo']}")
            self.volver()
        except Exception as e:
            messagebox.showerror("Error BD", f"No se pudo guardar el usuario: {e}")

class UsuariosFrame(tk.Frame):
    def __init__(self, master, volver):
        super().__init__(master, bg="#f3f3f7")
        self.pack(fill="both", expand=True)
        self.volver = volver

        tk.Label(self, text="Usuarios Registrados", font=("Segoe UI", 16, "bold"), bg="#f3f3f7").pack(pady=10)
        buscador_frame = tk.Frame(self, bg="#f3f3f7")
        buscador_frame.pack(fill="x", padx=22, pady=(0,10))
        tk.Label(buscador_frame, text="Buscar:", bg="#f3f3f7", font=("Segoe UI", 12)).pack(side=tk.LEFT)
        self.buscador_entry = tk.Entry(buscador_frame, font=("Segoe UI", 11), width=30)
        self.buscador_entry.pack(side=tk.LEFT, padx=12)
        tk.Button(buscador_frame, text="Buscar", command=self.buscar, bg="#4a90e2", fg="white",
                  font=("Segoe UI", 11, "bold"), width=8).pack(side=tk.LEFT)
        tk.Button(buscador_frame, text="Restablecer", command=self.cargar,
                  bg="#999999", fg="white", font=("Segoe UI", 10, "bold"), width=10).pack(side=tk.LEFT, padx=8)

        columns = ("nombre", "apellido", "ci", "celular", "correo", "foto")
        self.tree = ttk.Treeview(self, columns=columns, show="headings")
        for col in columns:
            self.tree.heading(col, text=col.capitalize())
            self.tree.column(col, width=100 if col != "foto" else 60)
        self.tree.pack(fill=tk.BOTH, expand=True)

        btns_frame = tk.Frame(self, bg="#f3f3f7")
        btns_frame.pack(fill="x", side=tk.BOTTOM, pady=12)

        boton_config = {
            "width": 11, 
            "height": 2, 
            "font": ("Segoe UI", 13, "bold")
        }
        tk.Button(btns_frame, text="Ver Foto", command=self.ver_foto,
                  bg="#4a90e2", fg="white", **boton_config).pack(side=tk.LEFT, padx=15)
        tk.Button(btns_frame, text="Editar", command=self.editar_usuario,
                  bg="#45b882", fg="white", **boton_config).pack(side=tk.LEFT, padx=40)
        tk.Button(btns_frame, text="Eliminar", command=self.eliminar_usuario,
                  bg="#e0393e", fg="white", **boton_config).pack(side=tk.LEFT, padx=60)
        tk.Button(btns_frame, text="Volver", command=self.volver,
                  bg="#999999", fg="white", **boton_config).pack(side=tk.RIGHT, padx=30)

        self.cargar()

    def cargar(self):
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()
        cursor.execute("SELECT id, nombre, apellido, ci, celular, correo, ruta_foto FROM Personas WHERE activo=1")
        rows = cursor.fetchall()
        self.tree.delete(*self.tree.get_children())
        for row in rows:
            (id_, nombre, apellido, ci, celular, correo, foto_bin) = row
            if foto_bin is not None and isinstance(foto_bin, (bytes, str, list, tuple)) and len(foto_bin) > 100:
                tiene_foto = "Sí"
            else:
                tiene_foto = "No"
            # Insertamos solo los datos visibles, pero guardamos el ID como iid
            self.tree.insert('', tk.END, iid=str(id_), values=(nombre, apellido, ci, celular, correo, tiene_foto))
        conn.close()

    def buscar(self):
        texto = self.buscador_entry.get().strip().lower()
        if not texto:
            self.cargar()
            return
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()
        cursor.execute("""
            SELECT id, nombre, apellido, ci, celular, correo, ruta_foto 
            FROM Personas
            WHERE activo=1 AND (LOWER(nombre) LIKE %s OR LOWER(apellido) LIKE %s OR ci LIKE %s)
        """, (f"%{texto}%", f"%{texto}%", f"%{texto}%"))
        rows = cursor.fetchall()
        self.tree.delete(*self.tree.get_children())
        for row in rows:
            (id_, nombre, apellido, ci, celular, correo, foto_bin) = row
            if foto_bin is not None and isinstance(foto_bin, (bytes, str, list, tuple)) and len(foto_bin) > 100:
                tiene_foto = "Sí"
            else:
                tiene_foto = "No"
            self.tree.insert('', tk.END, iid=str(id_), values=(nombre, apellido, ci, celular, correo, tiene_foto))
        conn.close()

    def ver_foto(self):
        sel = self.tree.selection()
        if not sel:
            messagebox.showwarning("Atención", "Seleccione un usuario.")
            return
        user_id = int(sel[0])
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()
        cursor.execute("SELECT ruta_foto FROM Personas WHERE id=%s", (user_id,))
        foto = cursor.fetchone()
        conn.close()
        if foto and foto[0]:  # type: ignore
            mostrar_foto_en_pantalla_completa(foto[0])  # type: ignore
        else:
            messagebox.showinfo("Sin foto", "Este usuario no tiene foto.")

    def editar_usuario(self):
        sel = self.tree.selection()
        if not sel:
            messagebox.showwarning("Atención", "Seleccione un usuario a editar.")
            return
        user_id = int(sel[0])
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()
        cursor.execute("SELECT nombre, apellido, ci, celular, correo FROM Personas WHERE id=%s", (user_id,))
        usuario = cursor.fetchone()
        conn.close()
        if not usuario:
            messagebox.showerror("Error", "Usuario no encontrado.")
            return
        nombre, apellido, ci, celular, correo = usuario
        edit_win = tk.Toplevel(self)
        edit_win.title("Editar usuario")
        edit_win.configure(bg="#f3f3f7")
        try:
            edit_win.transient(self.master)  # type: ignore
        except:
            pass
        tk.Label(edit_win, text="Editar usuario", font=("Segoe UI", 14, "bold"), bg="#f3f3f7").pack(pady=10)
        entries = {}
        for i, campo in enumerate(["Nombre", "Apellido", "C.I.", "Celular", "Correo"]):
            tk.Label(edit_win, text=campo, bg="#f3f3f7", font=("Segoe UI", 12)).pack()
            ent = tk.Entry(edit_win, font=("Segoe UI", 11), width=32)
            ent.pack(pady=2)
            ent.insert(0, [nombre, apellido, ci, celular, correo][i]) # type: ignore
            entries[campo] = ent

        def guardar_edicion():
            datos = {k: e.get().strip() for k, e in entries.items()}
            if not all(datos.values()):
                messagebox.showerror("Error", "Complete todos los campos.")
                return
            if not datos["C.I."].isdigit():
                messagebox.showerror("Error", "C.I. debe contener solo números.")
                return
            if not re.match(r'^\+?\d+$', datos["Celular"]):
                messagebox.showerror("Error", "Celular debe contener solo números y opcionalmente un '+' al inicio.")
                return
            if not datos["Correo"].endswith("@gmail.com"):
                messagebox.showerror("Error", "Correo debe ser una dirección válida y terminar con '@gmail.com'.")
                return
            conn = mysql.connector.connect(**DB_CONFIG)
            cursor = conn.cursor()
            cursor.execute("""
                UPDATE Personas 
                SET nombre=%s, apellido=%s, ci=%s, celular=%s, correo=%s 
                WHERE id=%s
            """, (datos["Nombre"], datos["Apellido"], datos["C.I."], datos["Celular"], datos["Correo"], user_id))
            conn.commit()
            conn.close()
            messagebox.showinfo("Éxito", "Usuario editado correctamente.")
            edit_win.destroy()
            self.cargar()
        
        tk.Button(edit_win, text="Guardar Cambios", command=guardar_edicion, bg="#4a90e2", fg="white",
                  font=("Segoe UI", 12, "bold"), width=16).pack(pady=12)

    def eliminar_usuario(self):
        sel = self.tree.selection()
        if not sel:
            messagebox.showwarning("Atención", "Seleccione un usuario a desactivar.")
            return
        user_id = int(sel[0])
        if not messagebox.askyesno("Confirmar", "¿Seguro que desea eliminar este usuario?"):
            return
        try:
            conn = mysql.connector.connect(**DB_CONFIG)
            cursor = conn.cursor()
            cursor.execute("UPDATE Personas SET activo=0 WHERE id=%s", (user_id,))
            conn.commit()
            cursor.close()
            conn.close()
            messagebox.showinfo("Éxito", "Usuario eliminado correctamente.")
            self.cargar()
        except Exception as e:
            messagebox.showerror("Error", f"No se pudo eliminar al usuario: {e}")

class RegistrosFrame(tk.Frame):
    def __init__(self, master, volver):
        super().__init__(master, bg="#f3f3f7")
        self.pack(fill="both", expand=True)
        self.volver = volver

        tk.Label(self, text="Registros de Ingreso", font=("Segoe UI", 16, "bold"), bg="#f3f3f7").pack(pady=10)

        columns = ("nombre_completo", "contraseña_ingresada", "exito", "timestamp", "temperatura")
        self.tree = ttk.Treeview(self, columns=columns, show="headings")
        self.tree.heading("nombre_completo", text="Nombre Completo")
        self.tree.heading("contraseña_ingresada", text="Contraseña Ingresada")
        self.tree.heading("exito", text="Éxito")
        self.tree.heading("timestamp", text="Fecha y Hora")
        self.tree.heading("temperatura", text="Temperatura (°C)")

        self.tree.column("nombre_completo", width=220)
        self.tree.column("contraseña_ingresada", width=160)
        self.tree.column("exito", width=80)
        self.tree.column("timestamp", width=160)
        self.tree.column("temperatura", width=110)
        self.tree.pack(fill=tk.BOTH, expand=True)

        crear_boton_ver_foto(self, self.ver_foto).pack(pady=10)
        boton_volver = crear_boton_volver(self, self.volver)
        boton_volver.pack(side=tk.RIGHT, padx=30, pady=10, anchor='se')

        self.cargar()

    def cargar(self):
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()
        cursor.execute("""
            SELECT r.id,
                COALESCE(CONCAT(p.nombre, ' ', p.apellido), 'Desconocido') AS nombre_completo,
                r.contraseña_ingresada,
                r.exito,
                r.timestamp,
                r.temperatura,
                r.foto
            FROM registros r
            LEFT JOIN Personas p ON r.persona_id = p.id
            ORDER BY r.timestamp DESC
        """)
        rows = cursor.fetchall()
        self.tree.delete(*self.tree.get_children())
        for row in rows:
            reg_id, nombre_completo, contraseña_ingresada, exito, timestamp, temperatura, foto = row
            exito_str = "Correcto" if exito else "Incorrecto"
            temperatura_str = f"{temperatura:.1f} °C" if temperatura is not None else "N/A"
            self.tree.insert('', tk.END, iid=str(reg_id),
                             values=(nombre_completo, contraseña_ingresada, exito_str, timestamp, temperatura_str))
        cursor.close()
        conn.close()

    def ver_foto(self):
        sel = self.tree.selection()
        if not sel:
            messagebox.showwarning("Atención", "Seleccione un registro.")
            return
        reg_id = int(sel[0])
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()
        cursor.execute("SELECT foto FROM registros WHERE id=%s", (reg_id,))
        foto = cursor.fetchone()
        cursor.close()
        conn.close()
        if foto and foto[0]:  # type: ignore
            mostrar_foto_en_pantalla_completa(foto[0])  # type: ignore
        else:
            messagebox.showinfo("Sin foto", "Este registro no tiene foto guardada.")


class PrincipalApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Sistema de Gestión - JACH'A TECH")
        self.geometry("1024x700")
        self.configure(bg="#f3f3f7")
        self.left_frame = tk.Frame(self, bg="#FFFFFF", width=220)
        self.left_frame.pack(side="left", fill="y")
        try:
            logo_img = Image.open("image.png").resize((110, 110), resample=RESAMPLE_MODE)
            self.logo_img_sidebar = ImageTk.PhotoImage(logo_img)
            tk.Label(self.left_frame, image=self.logo_img_sidebar, bg="#FFFFFF").pack(pady=(16, 10))
        except:
            tk.Label(self.left_frame, text="JACH'A TECH", font=("Arial", 18, "bold"), bg="#FFFFFF").pack(pady=(18, 8))
        ttk.Style().configure("Sidebar.TButton", font=("Segoe UI", 13), background="#4361ee", foreground="black", padding=(10, 12))
        ttk.Button(self.left_frame, text="Registrar Nuevo Usuario", command=self.mostrar_registro, style="Sidebar.TButton").pack(fill="x", pady=(10, 15), padx=20)
        ttk.Button(self.left_frame, text="Ver Usuarios Registrados", command=self.mostrar_usuarios, style="Sidebar.TButton").pack(fill="x", pady=15, padx=20)
        ttk.Button(self.left_frame, text="Ver Registros de Ingreso", command=self.mostrar_registros, style="Sidebar.TButton").pack(fill="x", pady=15, padx=20)
        self.content_frame = tk.Frame(self, bg="#f3f3f7")
        self.content_frame.pack(side="left", expand=True, fill="both")
        self.mostrar_inicio()


    def limpiar_contenido(self):
        for widget in self.content_frame.winfo_children():
            widget.destroy()


    def mostrar_inicio(self):
        self.limpiar_contenido()
        try:
            logo_img = Image.open("image.png").resize((500, 500), resample=RESAMPLE_MODE)
            self.logo_img_tk = ImageTk.PhotoImage(logo_img)
            tk.Label(self.content_frame, image=self.logo_img_tk, bg="#f3f3f7").pack(expand=True)
        except:
            tk.Label(self.content_frame, text="JACH'A TECH", font=("Arial", 40, "bold"), bg="#f3f3f7").pack(expand=True)


    def mostrar_registro(self):
        self.limpiar_contenido()
        RegistroFrame(self.content_frame, self.mostrar_inicio)


    def mostrar_usuarios(self):
        self.limpiar_contenido()
        UsuariosFrame(self.content_frame, self.mostrar_inicio)


    def mostrar_registros(self):
        self.limpiar_contenido()
        RegistrosFrame(self.content_frame, self.mostrar_inicio)


class LoginApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Login Admin")
        self.geometry("420x450")
        self.resizable(False, False)
        self.configure(bg="#f3f3f7")
        try:
            img = Image.open("image.png")
            img = img.resize((180, 180), resample=RESAMPLE_MODE)
            self.logo_img = ImageTk.PhotoImage(img)
            tk.Label(self, image=self.logo_img, bg="#f3f3f7").pack(pady=(15, 5))
        except:
            pass


        frm = tk.Frame(self, bg="#f3f3f7")
        frm.pack(pady=5)


        tk.Label(frm, text="Usuario:", font=("Segoe UI", 12), bg="#f3f3f7").grid(row=0, column=0, sticky="e", padx=10, pady=10)
        self.usuario_entry = tk.Entry(frm, font=("Segoe UI", 12), width=23)
        self.usuario_entry.grid(row=0, column=1, padx=10, pady=10)


        tk.Label(frm, text="Contraseña:", font=("Segoe UI", 12), bg="#f3f3f7").grid(row=1, column=0, sticky="e", padx=10, pady=10)
        self.pass_entry = tk.Entry(frm, show="*", font=("Segoe UI", 12), width=23)
        self.pass_entry.grid(row=1, column=1, padx=10, pady=10)


        ttk.Style().configure("Red.TButton", foreground="black", background="#4a90e2", font=("Segoe UI", 13, "bold"), padding=8)
        ttk.Button(self, text="Entrar", command=self.verificar_login, style="Red.TButton").pack(pady=20)


    def verificar_login(self):
        usuario = self.usuario_entry.get()
        contraseña = self.pass_entry.get()
        if usuario == "admin" and contraseña == "123456789":
            self.destroy()
            PrincipalApp().mainloop()
        else:
            messagebox.showerror("Error", "Usuario o contraseña incorrectos")

if __name__ == "__main__":
    LoginApp().mainloop()
