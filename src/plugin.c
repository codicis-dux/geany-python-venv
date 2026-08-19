/*
 * Python Venv - Geany plugin
 *
 * Copyright (C) 2026 dr
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <geanyplugin.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>

#define CONFIG_DIR_NAME  "python-venv"
#define CONFIG_FILE_NAME "python-venv.conf"

#define MAX_SCAN_DEPTH 4

typedef struct
{
    gchar *name;
    gchar *path;
} Venv;

static GtkWidget *execute_menu_item = NULL;
static GtkWidget *execute_submenu = NULL;

static gchar *config_file = NULL;

static GPtrArray *scan_roots = NULL;
static GPtrArray *venvs = NULL;


/* ============================================================
 * Utilitaires
 * ============================================================ */

static void venv_free(gpointer data)
{
    Venv *venv = data;

    if (!venv)
        return;

    g_free(venv->name);
    g_free(venv->path);
    g_free(venv);
}


static void ensure_arrays(void)
{
    if (!scan_roots)
        scan_roots = g_ptr_array_new_with_free_func(g_free);

    if (!venvs)
        venvs = g_ptr_array_new_with_free_func(venv_free);
}


/* ============================================================
 * Détection des virtualenvs
 * ============================================================ */

static gboolean is_venv(const gchar *path)
{
    if (!path)
        return FALSE;

    gchar *python =
        g_build_filename(path, "bin", "python", NULL);

    gboolean result =
        g_file_test(python, G_FILE_TEST_IS_EXECUTABLE);

    g_free(python);

    return result;
}


static void add_venv(const gchar *path)
{
    if (!path || !is_venv(path))
        return;

    for (guint i = 0; i < venvs->len; i++)
    {
        Venv *existing =
            g_ptr_array_index(venvs, i);

        if (g_strcmp0(existing->path, path) == 0)
            return;
    }

    Venv *venv = g_new0(Venv, 1);

    venv->path = g_strdup(path);
    venv->name = g_path_get_basename(path);

    g_ptr_array_add(venvs, venv);
}


static void scan_directory(
    const gchar *directory,
    guint depth)
{
    if (!directory)
        return;

    if (depth > MAX_SCAN_DEPTH)
        return;

    if (!g_file_test(directory, G_FILE_TEST_IS_DIR))
        return;

    if (depth > 0 && is_venv(directory))
    {
        add_venv(directory);
        return;
    }

    GDir *dir =
        g_dir_open(directory, 0, NULL);

    if (!dir)
        return;

    const gchar *name;

    while ((name = g_dir_read_name(dir)) != NULL)
    {
        if (name[0] == '.')
            continue;

        if (g_strcmp0(name, "__pycache__") == 0 ||
            g_strcmp0(name, "node_modules") == 0 ||
            g_strcmp0(name, ".git") == 0)
        {
            continue;
        }

        gchar *child =
            g_build_filename(directory, name, NULL);

        if (g_file_test(child, G_FILE_TEST_IS_DIR))
            scan_directory(child, depth + 1);

        g_free(child);
    }

    g_dir_close(dir);
}


static gint compare_venvs(
    gconstpointer a,
    gconstpointer b)
{
    const Venv *va = *(Venv **)a;
    const Venv *vb = *(Venv **)b;

    gint result =
        g_ascii_strcasecmp(va->name, vb->name);

    if (result == 0)
    {
        result =
            g_ascii_strcasecmp(va->path, vb->path);
    }

    return result;
}


static void discover_venvs(void)
{
    ensure_arrays();

    g_ptr_array_set_size(venvs, 0);

    for (guint i = 0; i < scan_roots->len; i++)
    {
        const gchar *root =
            g_ptr_array_index(scan_roots, i);

        scan_directory(root, 0);
    }

    /*
     * Détection automatique du .venv / venv
     * du projet courant.
     */
    GeanyDocument *doc =
        document_get_current();

    if (doc && doc->file_name)
    {
        gchar *project_dir =
            g_path_get_dirname(doc->file_name);

        gchar *dotvenv =
            g_build_filename(project_dir, ".venv", NULL);

        gchar *venv =
            g_build_filename(project_dir, "venv", NULL);

        add_venv(dotvenv);
        add_venv(venv);

        g_free(dotvenv);
        g_free(venv);
        g_free(project_dir);
    }

    g_ptr_array_sort(venvs, compare_venvs);
}


/* ============================================================
 * Configuration
 * ============================================================ */

static void save_config(void)
{
    if (!config_file)
        return;

    GKeyFile *keyfile =
        g_key_file_new();

    if (scan_roots && scan_roots->len > 0)
    {
        gchar **roots =
            g_new0(gchar *, scan_roots->len + 1);

        for (guint i = 0; i < scan_roots->len; i++)
        {
            roots[i] =
                g_strdup(
                    g_ptr_array_index(scan_roots, i));
        }

        g_key_file_set_string_list(
            keyfile,
            "Python Venv",
            "scan_roots",
            (const gchar * const *)roots,
            scan_roots->len);

        g_strfreev(roots);
    }

    gchar *data =
        g_key_file_to_data(keyfile, NULL, NULL);

    gchar *dir =
        g_path_get_dirname(config_file);

    if (g_mkdir_with_parents(dir, 0700) == 0)
    {
        g_file_set_contents(
            config_file,
            data,
            -1,
            NULL);
    }

    g_free(dir);
    g_free(data);
    g_key_file_free(keyfile);
}


static void load_config(GeanyPlugin *plugin)
{
    ensure_arrays();

    config_file =
        g_build_filename(
            plugin->geany_data->app->configdir,
            "plugins",
            CONFIG_DIR_NAME,
            CONFIG_FILE_NAME,
            NULL);

    GKeyFile *keyfile =
        g_key_file_new();

    gboolean loaded =
        g_key_file_load_from_file(
            keyfile,
            config_file,
            G_KEY_FILE_NONE,
            NULL);

    if (loaded)
    {
        gsize length = 0;

        gchar **roots =
            g_key_file_get_string_list(
                keyfile,
                "Python Venv",
                "scan_roots",
                &length,
                NULL);

        if (roots)
        {
            for (gsize i = 0; i < length; i++)
            {
                if (roots[i] && *roots[i])
                {
                    g_ptr_array_add(
                        scan_roots,
                        g_strdup(roots[i]));
                }
            }

            g_strfreev(roots);
        }
    }

    g_key_file_free(keyfile);

    if (scan_roots->len == 0)
    {
        g_ptr_array_add(
            scan_roots,
            g_build_filename(
                g_get_home_dir(),
                "venvs",
                NULL));

        g_ptr_array_add(
            scan_roots,
            g_build_filename(
                g_get_home_dir(),
                ".virtualenvs",
                NULL));

        g_ptr_array_add(
            scan_roots,
            g_build_filename(
                g_get_home_dir(),
                "Documents",
                "venvs",
                NULL));
    }
}


/* ============================================================
 * Exécution dans terminal externe
 * ============================================================ */

static void execute_venv_path(
    const gchar *venv_path)
{
    if (!venv_path || !*venv_path)
        return;

    gchar *python =
        g_build_filename(
            venv_path,
            "bin",
            "python",
            NULL);

    if (!g_file_test(
            python,
            G_FILE_TEST_IS_EXECUTABLE))
    {
        GtkWidget *dialog =
            gtk_message_dialog_new(
                NULL,
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_CLOSE,
                "Python introuvable dans le virtualenv :\n%s",
                python);

        gtk_window_set_title(
            GTK_WINDOW(dialog),
            "Execute Venv");

        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        g_free(python);
        return;
    }

    GeanyDocument *doc =
        document_get_current();

    if (!doc || !doc->file_name)
    {
        GtkWidget *dialog =
            gtk_message_dialog_new(
                NULL,
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_CLOSE,
                "Aucun fichier n'est actuellement ouvert.");

        gtk_window_set_title(
            GTK_WINDOW(dialog),
            "Execute Venv");

        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        g_free(python);
        return;
    }

    if (!g_str_has_suffix(doc->file_name, ".py"))
    {
        GtkWidget *dialog =
            gtk_message_dialog_new(
                NULL,
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_CLOSE,
                "Le fichier courant n'est pas un fichier Python.");

        gtk_window_set_title(
            GTK_WINDOW(dialog),
            "Execute Venv");

        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        g_free(python);
        return;
    }

    gchar *working_dir =
        g_path_get_dirname(doc->file_name);

    gchar *qpython =
        g_shell_quote(python);

    gchar *qscript =
        g_shell_quote(doc->file_name);

    gchar *command =
        g_strdup_printf(
            "%s %s; "
            "status=$?; "
            "printf '\\n\\n=== Execute Venv : fin (code %%d) ===\\n' \"$status\"; "
            "printf 'Appuyez sur Entrée pour fermer...\\n'; "
            "read dummy",
            qpython,
            qscript);

    gchar *argv[] =
    {
        "x-terminal-emulator",
        "-e",
        "sh",
        "-c",
        command,
        NULL
    };

    GError *error = NULL;

    gboolean ok =
        g_spawn_async(
            working_dir,
            argv,
            NULL,
            G_SPAWN_SEARCH_PATH,
            NULL,
            NULL,
            NULL,
            &error);

    if (!ok)
    {
        GtkWidget *dialog =
            gtk_message_dialog_new(
                NULL,
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_CLOSE,
                "Impossible de lancer le terminal.");

        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(dialog),
            "%s",
            error
                ? error->message
                : "Erreur inconnue");

        gtk_window_set_title(
            GTK_WINDOW(dialog),
            "Execute Venv");

        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        if (error)
            g_error_free(error);
    }

    g_free(qpython);
    g_free(qscript);
    g_free(command);
    g_free(working_dir);
    g_free(python);
}


/* ============================================================
 * Menu
 * ============================================================ */

static void venv_menu_activate(
    GtkMenuItem *item,
    gpointer user_data)
{
    (void)item;

    execute_venv_path(
        (const gchar *)user_data);
}


static void rebuild_venv_menu(void);


static void refresh_activate(
    GtkMenuItem *item,
    gpointer user_data)
{
    (void)item;
    (void)user_data;

    rebuild_venv_menu();
}


/* ============================================================
 * Configuration : sélection d'une ligne
 * ============================================================ */

static void config_selection_changed(
    GtkTreeSelection *selection,
    gpointer user_data)
{
    GtkWidget *remove_button =
        GTK_WIDGET(user_data);

    GtkTreeModel *model = NULL;
    GtkTreeIter iter;

    gboolean selected =
        gtk_tree_selection_get_selected(
            selection,
            &model,
            &iter);

    gtk_widget_set_sensitive(
        remove_button,
        selected);
}


/* ============================================================
 * Ajouter
 * ============================================================ */

static void config_add_clicked(
    GtkButton *button,
    gpointer user_data)
{
    (void)button;

    GtkWidget *treeview =
        GTK_WIDGET(user_data);

    GtkListStore *store =
        GTK_LIST_STORE(
            gtk_tree_view_get_model(
                GTK_TREE_VIEW(treeview)));

    GtkWidget *dialog =
        gtk_file_chooser_dialog_new(
            "Choisir un répertoire à scanner",
            NULL,
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            "_Annuler",
            GTK_RESPONSE_CANCEL,
            "_Sélectionner",
            GTK_RESPONSE_ACCEPT,
            NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog))
        == GTK_RESPONSE_ACCEPT)
    {
        gchar *selected =
            gtk_file_chooser_get_filename(
                GTK_FILE_CHOOSER(dialog));

        if (selected && *selected)
        {
            gboolean exists = FALSE;

            GtkTreeIter iter;

            gboolean valid =
                gtk_tree_model_get_iter_first(
                    GTK_TREE_MODEL(store),
                    &iter);

            while (valid)
            {
                gchar *path = NULL;

                gtk_tree_model_get(
                    GTK_TREE_MODEL(store),
                    &iter,
                    0,
                    &path,
                    -1);

                if (g_strcmp0(path, selected) == 0)
                {
                    exists = TRUE;
                    g_free(path);
                    break;
                }

                g_free(path);

                valid =
                    gtk_tree_model_iter_next(
                        GTK_TREE_MODEL(store),
                        &iter);
            }

            if (!exists)
            {
                gtk_list_store_append(
                    store,
                    &iter);

                gtk_list_store_set(
                    store,
                    &iter,
                    0,
                    selected,
                    -1);
            }
        }

        g_free(selected);
    }

    gtk_widget_destroy(dialog);
}


/* ============================================================
 * Supprimer
 * ============================================================ */

static void config_remove_clicked(
    GtkButton *button,
    gpointer user_data)
{
    (void)button;

    GtkWidget *treeview =
        GTK_WIDGET(user_data);

    GtkTreeSelection *selection =
        gtk_tree_view_get_selection(
            GTK_TREE_VIEW(treeview));

    GtkTreeModel *model = NULL;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(
            selection,
            &model,
            &iter))
    {
        return;
    }

    /*
     * IMPORTANT :
     * gtk_list_store_remove() retire uniquement
     * le chemin de notre liste.
     *
     * Aucun fichier ni répertoire du disque
     * n'est supprimé.
     */
    gtk_list_store_remove(
        GTK_LIST_STORE(model),
        &iter);
}


/* ============================================================
 * Touche Suppr
 * ============================================================ */

static gboolean config_key_press(
    GtkWidget *widget,
    GdkEventKey *event,
    gpointer user_data)
{
    (void)widget;

    if (event->keyval != GDK_KEY_Delete &&
        event->keyval != GDK_KEY_KP_Delete)
    {
        return FALSE;
    }

    config_remove_clicked(
        NULL,
        user_data);

    return TRUE;
}


/* ============================================================
 * Enregistrer
 * ============================================================ */

static void config_ok_clicked(
    GtkButton *button,
    gpointer user_data)
{
    (void)button;

    GtkDialog *dialog =
        GTK_DIALOG(user_data);

    GtkWidget *treeview =
        g_object_get_data(
            G_OBJECT(dialog),
            "venv-treeview");

    GtkListStore *store =
        GTK_LIST_STORE(
            gtk_tree_view_get_model(
                GTK_TREE_VIEW(treeview)));

    g_ptr_array_set_size(
        scan_roots,
        0);

    GtkTreeIter iter;

    gboolean valid =
        gtk_tree_model_get_iter_first(
            GTK_TREE_MODEL(store),
            &iter);

    while (valid)
    {
        gchar *path = NULL;

        gtk_tree_model_get(
            GTK_TREE_MODEL(store),
            &iter,
            0,
            &path,
            -1);

        if (path && *path)
        {
            g_ptr_array_add(
                scan_roots,
                path);
        }
        else
        {
            g_free(path);
        }

        valid =
            gtk_tree_model_iter_next(
                GTK_TREE_MODEL(store),
                &iter);
    }

    save_config();

    gtk_widget_destroy(
        GTK_WIDGET(dialog));

    rebuild_venv_menu();
}


static void config_cancel_clicked(
    GtkButton *button,
    gpointer user_data)
{
    (void)button;

    gtk_widget_destroy(
        GTK_WIDGET(user_data));
}


/* ============================================================
 * Fenêtre de configuration
 * ============================================================ */

static void show_configuration_dialog(void)
{
    GtkWidget *dialog =
        gtk_dialog_new();

    gtk_window_set_title(
        GTK_WINDOW(dialog),
        "Python Venv — Répertoires");

    gtk_window_set_default_size(
        GTK_WINDOW(dialog),
        700,
        450);

    gtk_window_set_modal(
        GTK_WINDOW(dialog),
        TRUE);


    GtkWidget *content =
        gtk_dialog_get_content_area(
            GTK_DIALOG(dialog));


    GtkWidget *label =
        gtk_label_new(
            "Répertoires dans lesquels rechercher les virtualenvs :");

    gtk_label_set_xalign(
        GTK_LABEL(label),
        0.0);


    gtk_box_pack_start(
        GTK_BOX(content),
        label,
        FALSE,
        FALSE,
        8);


    /*
     * Liste des répertoires.
     */
    GtkListStore *store =
        gtk_list_store_new(
            1,
            G_TYPE_STRING);


    GtkWidget *treeview =
        gtk_tree_view_new_with_model(
            GTK_TREE_MODEL(store));

    g_object_unref(store);


    GtkCellRenderer *renderer =
        gtk_cell_renderer_text_new();


    GtkTreeViewColumn *column =
        gtk_tree_view_column_new_with_attributes(
            "Répertoire",
            renderer,
            "text",
            0,
            NULL);


    gtk_tree_view_append_column(
        GTK_TREE_VIEW(treeview),
        column);


    gtk_tree_view_set_headers_visible(
        GTK_TREE_VIEW(treeview),
        TRUE);


    gtk_tree_view_set_enable_search(
        GTK_TREE_VIEW(treeview),
        FALSE);


    GtkTreeSelection *selection =
        gtk_tree_view_get_selection(
            GTK_TREE_VIEW(treeview));


    GtkWidget *scroll =
        gtk_scrolled_window_new(
            NULL,
            NULL);

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC);


    gtk_container_add(
        GTK_CONTAINER(scroll),
        treeview);


    gtk_box_pack_start(
        GTK_BOX(content),
        scroll,
        TRUE,
        TRUE,
        8);


    /*
     * Remplir la liste.
     */
    for (guint i = 0;
         i < scan_roots->len;
         i++)
    {
        GtkTreeIter iter;

        gtk_list_store_append(
            store,
            &iter);

        gtk_list_store_set(
            store,
            &iter,
            0,
            g_ptr_array_index(
                scan_roots,
                i),
            -1);
    }


    /*
     * Boutons Ajouter / Supprimer.
     */
    GtkWidget *actions =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            6);

    GtkWidget *add =
        gtk_button_new_with_label(
            "Ajouter un répertoire...");

    GtkWidget *remove =
        gtk_button_new_with_label(
            "Supprimer");


    gtk_widget_set_sensitive(
        remove,
        FALSE);


    gtk_box_pack_start(
        GTK_BOX(actions),
        add,
        FALSE,
        FALSE,
        0);

    gtk_box_pack_start(
        GTK_BOX(actions),
        remove,
        FALSE,
        FALSE,
        0);


    gtk_box_pack_start(
        GTK_BOX(content),
        actions,
        FALSE,
        FALSE,
        8);


    /*
     * Info.
     */
    GtkWidget *info =
        gtk_label_new(
            "Sélectionnez un répertoire puis cliquez sur "
            "\"Supprimer\", ou utilisez la touche Suppr.\n"
            "La suppression retire uniquement le chemin de la "
            "configuration : le répertoire du disque n'est pas supprimé.");

    gtk_label_set_xalign(
        GTK_LABEL(info),
        0.0);

    gtk_box_pack_start(
        GTK_BOX(content),
        info,
        FALSE,
        FALSE,
        8);


    /*
     * Boutons de dialogue.
     */
    GtkWidget *buttons =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            6);

    gtk_widget_set_halign(
        buttons,
        GTK_ALIGN_END);


    GtkWidget *cancel =
        gtk_button_new_with_label(
            "Annuler");

    GtkWidget *ok =
        gtk_button_new_with_label(
            "Enregistrer");


    gtk_box_pack_start(
        GTK_BOX(buttons),
        cancel,
        FALSE,
        FALSE,
        0);

    gtk_box_pack_start(
        GTK_BOX(buttons),
        ok,
        FALSE,
        FALSE,
        0);


    gtk_box_pack_start(
        GTK_BOX(content),
        buttons,
        FALSE,
        FALSE,
        8);


    /*
     * Signaux.
     */
    g_signal_connect(
        add,
        "clicked",
        G_CALLBACK(config_add_clicked),
        treeview);


    g_signal_connect(
        remove,
        "clicked",
        G_CALLBACK(config_remove_clicked),
        treeview);


    g_signal_connect(
        selection,
        "changed",
        G_CALLBACK(config_selection_changed),
        remove);


    g_signal_connect(
        treeview,
        "key-press-event",
        G_CALLBACK(config_key_press),
        treeview);


    g_signal_connect(
        ok,
        "clicked",
        G_CALLBACK(config_ok_clicked),
        dialog);


    g_signal_connect(
        cancel,
        "clicked",
        G_CALLBACK(config_cancel_clicked),
        dialog);


    g_object_set_data(
        G_OBJECT(dialog),
        "venv-treeview",
        treeview);


    gtk_widget_show_all(dialog);
}


/* ============================================================
 * Reconstruction du sous-menu
 * ============================================================ */

static void rebuild_venv_menu(void)
{
    if (!execute_submenu)
        return;

    GList *children =
        gtk_container_get_children(
            GTK_CONTAINER(execute_submenu));

    for (GList *l = children;
         l;
         l = l->next)
    {
        gtk_widget_destroy(
            GTK_WIDGET(l->data));
    }

    g_list_free(children);


    discover_venvs();


    if (venvs->len == 0)
    {
        GtkWidget *item =
            gtk_menu_item_new_with_label(
                "(aucun virtualenv trouvé)");

        gtk_widget_set_sensitive(
            item,
            FALSE);

        gtk_menu_shell_append(
            GTK_MENU_SHELL(execute_submenu),
            item);

        gtk_widget_show(item);
    }
    else
    {
        for (guint i = 0;
             i < venvs->len;
             i++)
        {
            Venv *venv =
                g_ptr_array_index(
                    venvs,
                    i);

            GtkWidget *item =
                gtk_menu_item_new_with_label(
                    venv->name);

            gtk_widget_set_tooltip_text(
                item,
                venv->path);

            g_signal_connect(
                item,
                "activate",
                G_CALLBACK(venv_menu_activate),
                venv->path);

            gtk_menu_shell_append(
                GTK_MENU_SHELL(execute_submenu),
                item);

            gtk_widget_show(item);
        }
    }


    GtkWidget *separator =
        gtk_separator_menu_item_new();

    gtk_menu_shell_append(
        GTK_MENU_SHELL(execute_submenu),
        separator);

    gtk_widget_show(separator);


    GtkWidget *refresh =
        gtk_menu_item_new_with_label(
            "Actualiser les venvs");

    g_signal_connect(
        refresh,
        "activate",
        G_CALLBACK(refresh_activate),
        NULL);

    gtk_menu_shell_append(
        GTK_MENU_SHELL(execute_submenu),
        refresh);

    gtk_widget_show(refresh);


    GtkWidget *configure =
        gtk_menu_item_new_with_label(
            "Configurer les répertoires...");

    g_signal_connect(
        configure,
        "activate",
        G_CALLBACK(show_configuration_dialog),
        NULL);

    gtk_menu_shell_append(
        GTK_MENU_SHELL(execute_submenu),
        configure);

    gtk_widget_show(configure);
}


/* ============================================================
 * Initialisation
 * ============================================================ */

static gboolean venv_init(
    GeanyPlugin *plugin,
    gpointer pdata)
{
    (void)pdata;

    load_config(plugin);

    execute_menu_item =
        gtk_menu_item_new_with_label(
            "Execute Venv");

    execute_submenu =
        gtk_menu_new();

    gtk_menu_item_set_submenu(
        GTK_MENU_ITEM(execute_menu_item),
        execute_submenu);

    gtk_menu_shell_append(
        GTK_MENU_SHELL(
            plugin->geany_data
                ->main_widgets
                ->tools_menu),
        execute_menu_item);

    gtk_widget_show(
        execute_menu_item);

    rebuild_venv_menu();

    return TRUE;
}


/* ============================================================
 * Nettoyage
 * ============================================================ */

static void venv_cleanup(
    GeanyPlugin *plugin,
    gpointer pdata)
{
    (void)plugin;
    (void)pdata;

    if (execute_menu_item)
    {
        gtk_widget_destroy(
            execute_menu_item);

        execute_menu_item = NULL;
        execute_submenu = NULL;
    }

    if (venvs)
    {
        g_ptr_array_free(
            venvs,
            TRUE);

        venvs = NULL;
    }

    if (scan_roots)
    {
        g_ptr_array_free(
            scan_roots,
            TRUE);

        scan_roots = NULL;
    }

    g_free(config_file);
    config_file = NULL;
}


/* ============================================================
 * Aide
 * ============================================================ */

static void venv_help(
    GeanyPlugin *plugin,
    gpointer pdata)
{
    (void)plugin;
    (void)pdata;

    GtkWidget *dialog =
        gtk_message_dialog_new(
            NULL,
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_CLOSE,
            "Python Venv\n\n"
            "Détecte automatiquement les virtualenvs "
            "et exécute le fichier Python courant "
            "avec le virtualenv sélectionné.");

    gtk_window_set_title(
        GTK_WINDOW(dialog),
        "Python Venv");

    gtk_dialog_run(
        GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);
}


/* ============================================================
 * Point d'entrée Geany
 * ============================================================ */

G_MODULE_EXPORT
void geany_load_module(
    GeanyPlugin *plugin)
{
    plugin->info->name =
        "Python Venv";

    plugin->info->description =
        "Détecte et exécute les fichiers Python "
        "avec plusieurs virtualenvs.";

    plugin->info->version =
        "0.5";

    plugin->info->author =
        "dr";

    plugin->funcs->init =
        venv_init;

    plugin->funcs->cleanup =
        venv_cleanup;

    plugin->funcs->configure =
        NULL;

    plugin->funcs->help =
        venv_help;

    GEANY_PLUGIN_REGISTER(plugin, 225);
}
