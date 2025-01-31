/*
 * GAME SCENE
 * Copyright © 2018+ Ángel Rodríguez Ballesteros
 *
 * Distributed under the Boost Software License, version  1.0
 * See documents/LICENSE.TXT or www.boost.org/LICENSE_1_0.txt
 *
 * angel.rodriguez@esne.edu
 */

#include "Game_Scene.hpp"
#include "Menu_Scene.hpp"

#include <cstdlib>
#include <basics/Canvas>
#include <basics/Director>

using namespace basics;
using namespace std;

namespace example
{
    // ---------------------------------------------------------------------------------------------
    // ID y ruta de las texturas que se deben cargar para esta escena. La textura con el mensaje de
    // carga está la primera para poder dibujarla cuanto antes:

    Game_Scene::Texture_Data Game_Scene::textures_data[] =
            {
                    { ID(loading),    "game-scene/loading.png"        },
                    { ID(hbar),       "game-scene/horizontal-bar.png" },
                    { ID(flappy),     "game-scene/flappy.png"         },
                    { ID(top),        "game-scene/top.png"            },
                    { ID(bottom),     "game-scene/bottom.png"         },
                    { ID(exit),       "game-scene/exit.png"           },

            };

    // Pâra determinar el número de items en el array textures_data, se divide el tamaño en bytes
    // del array completo entre el tamaño en bytes de un item:

    unsigned Game_Scene::textures_count = sizeof(textures_data) / sizeof(Texture_Data);

    // ---------------------------------------------------------------------------------------------

    Game_Scene::Game_Scene()
    {
        // Se establece la resolución virtual (independiente de la resolución virtual del dispositivo).
        // En este caso no se hace ajuste de aspect ratio, por lo que puede haber distorsión cuando
        // el aspect ratio real de la pantalla del dispositivo es distinto.

        canvas_width  =  720;
        canvas_height = 1280;

        pipepos = canvas_height / 2;
        birdpos = canvas_height / 2;

        speed = 3.f;
        go = false;

        // Se inicia la semilla del generador de números aleatorios:

        srand (unsigned(time(nullptr)));

        // Se inicializan otros atributos:

        initialize ();
    }

    // ---------------------------------------------------------------------------------------------
    // Algunos atributos se inicializan en este método en lugar de hacerlo en el constructor porque
    // este método puede ser llamado más veces para restablecer el estado de la escena y el constructor
    // solo se invoca una vez.

    bool Game_Scene::initialize ()
    {
        state     = LOADING;
        suspended = true;
        gameplay  = UNINITIALIZED;

        return true;
    }

    // ---------------------------------------------------------------------------------------------

    void Game_Scene::suspend ()
    {
        suspended = true;               // Se marca que la escena ha pasado a primer plano
    }

    // ---------------------------------------------------------------------------------------------

    void Game_Scene::resume ()
    {
        suspended = false;              // Se marca que la escena ha pasado a segundo plano
    }

    // ---------------------------------------------------------------------------------------------

    void Game_Scene::handle (Event & event)
    {
        if (state == RUNNING)               // Se descartan los eventos cuando la escena está LOADING
        {
            if (gameplay == WAITING_TO_START)
            {
                start_playing ();           // Se empieza a jugar cuando el usuario toca la pantalla por primera vez
            }
            else switch (event.id)
            {
                case ID(touch-started):     // El usuario toca la pantalla
                {
                    float touchX = *event[ID(x) ].as< var::Float > ();
                    float touchY = *event[ID(y) ].as< var::Float > ();

                    if(touchX > exit_x->get_position_x() && touchX < exit_x->get_width() && touchY > exit_x->get_position_y() && touchY < exit_x->get_height())
                    {
                        director.run_scene (shared_ptr< Scene >(new Menu_Scene));
                    }
                    else
                    {
                        birdjump = true;
                        go = true;
                        timer.reset();
                    }
                }
                case ID(touch-moved):
                {

                }
                case ID(touch-ended):       // El usuario deja de tocar la pantalla
                {

                }
            }
        }
    }

    // ---------------------------------------------------------------------------------------------

    void Game_Scene::update (float time)
    {
        if (!suspended) switch (state)
            {
                case LOADING: load_textures  ();     break;
                case RUNNING: run_simulation (time); break;
                case ERROR:   break;
            }
    }

    // ---------------------------------------------------------------------------------------------

    void Game_Scene::render (Context & context)
    {
        if (!suspended)
        {
            // El canvas se puede haber creado previamente, en cuyo caso solo hay que pedirlo:

            Canvas * canvas = context->get_renderer< Canvas > (ID(canvas));

            // Si no se ha creado previamente, hay que crearlo una vez:

            if (!canvas)
            {
                canvas = Canvas::create (ID(canvas), context, {{ canvas_width, canvas_height }});
            }

            // Si el canvas se ha podido obtener o crear, se puede dibujar con él:

            if (canvas)
            {
                canvas->clear ();

                switch (state)
                {
                    case LOADING: render_loading   (*canvas); break;
                    case RUNNING: render_playfield (*canvas); break;
                    case ERROR:   break;
                }
            }
        }
    }

    // ---------------------------------------------------------------------------------------------
    // En este método solo se carga una textura por fotograma para poder pausar la carga si el
    // juego pasa a segundo plano inesperadamente. Otro aspecto interesante es que la carga no
    // comienza hasta que la escena se inicia para así tener la posibilidad de mostrar al usuario
    // que la carga está en curso en lugar de tener una pantalla en negro que no responde durante
    // un tiempo.

    void Game_Scene::load_textures ()
    {
        if (textures.size () < textures_count)          // Si quedan texturas por cargar...
        {
            // Las texturas se cargan y se suben al contexto gráfico, por lo que es necesario disponer
            // de uno:

            Graphics_Context::Accessor context = director.lock_graphics_context ();

            if (context)
            {
                // Se carga la siguiente textura (textures.size() indica cuántas llevamos cargadas):

                Texture_Data   & texture_data = textures_data[textures.size ()];
                Texture_Handle & texture      = textures[texture_data.id] = Texture_2D::create (texture_data.id, context, texture_data.path);

                // Se comprueba si la textura se ha podido cargar correctamente:

                if (texture) context->add (texture); else state = ERROR;

                // Cuando se han terminado de cargar todas las texturas se pueden crear los sprites que
                // las usarán e iniciar el juego:
            }
        }
        else
        if (timer.get_elapsed_seconds () > 1.f)         // Si las texturas se han cargado muy rápido
        {                                               // se espera un segundo desde el inicio de
            create_sprites ();                          // la carga antes de pasar al juego para que
            restart_game   ();                          // el mensaje de carga no aparezca y desaparezca
            // demasiado rápido.
            state = RUNNING;
        }
    }

    // ---------------------------------------------------------------------------------------------

    void Game_Scene::create_sprites ()
    {
        // Se crean y configuran los sprites del fondo:

        Sprite_Handle    top_bar(new Sprite( textures[ID(hbar)].get () ));
        Sprite_Handle bottom_bar(new Sprite( textures[ID(hbar)].get () ));

        top_bar->set_anchor   (TOP | LEFT);
        top_bar->set_position ({ 0, canvas_height });
        bottom_bar->set_anchor   (BOTTOM | LEFT);
        bottom_bar->set_position ({ 0, 0 });

        sprites.push_back (   top_bar);
        sprites.push_back (bottom_bar);

        // Se guardan punteros a los sprites que se van a usar frecuentemente:

        top_border    =             top_bar.get ();
        bottom_border =          bottom_bar.get ();

        Sprite_Handle bird_handle  (new Sprite( textures[ID(flappy)].get() ));
        Sprite_Handle top_handle   (new Sprite( textures[ID(top)].get()    ));
        Sprite_Handle bottom_handle(new Sprite( textures[ID(bottom)].get() ));
        Sprite_Handle exit_handle  (new Sprite( textures[ID(exit)].get()   ));

        sprites.push_back(bird_handle);
        sprites.push_back(top_handle);
        sprites.push_back(bottom_handle);
        sprites.push_back(exit_handle);

        bird        = bird_handle.get();
        toppipe     = top_handle.get();
        bottompipe  = bottom_handle.get();
        exit_x      = exit_handle.get();

        exit_x->set_position({canvas_width - 100, canvas_height - 100});
        exit_x->set_scale(0.2f);
    }

    // ---------------------------------------------------------------------------------------------
    // Juando el juego se inicia por primera vez o cuando se reinicia porque un jugador pierde, se
    // llama a este método para restablecer la posición y velocidad de los sprites:

    void Game_Scene::restart_game()
    {
        bird->set_position({bird->get_width() * 3.f , birdpos});
        bird->set_speed_y (0.f);
        bird->set_scale(4.5f);

        toppipe->set_position({canvas_width - toppipe->get_width(), pipepos + 400.f});
        toppipe->set_speed_x(0.f);

        bottompipe->set_position({canvas_width - bottompipe->get_width(), pipepos - 400.f});
        bottompipe->set_speed_x(0.f);

        pipepos = canvas_height / 2;

        gameplay = WAITING_TO_START;
    }

    // ---------------------------------------------------------------------------------------------

    void Game_Scene::start_playing ()
    {
        gameplay = PLAYING;
    }

    // ---------------------------------------------------------------------------------------------

    void Game_Scene::run_simulation (float time)
    {
        // Se actualiza el estado de todos los sprites:

        for (auto & sprite : sprites)
        {
            sprite->update (time);
        }

        if(go)
        {
            update_ai   ();
            update_user ();
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Usando un algoritmo sencillo se controla automáticamente el comportamiento del jugador
    // izquierdo.

    void Game_Scene::update_ai ()
    {
        toppipe->set_position_x(toppipe->get_position_x() - speed);
        bottompipe->set_position_x(bottompipe->get_position_x() - speed);

        if(toppipe->get_position_x() < 0)
        {
            pipepos = float( ((canvas_height / 2) - 200)+ rand () % (((canvas_height / 2) + 200) - ((canvas_height / 2) - 200)) );

            toppipe->set_position_x(canvas_width);
            toppipe->set_position_y(pipepos + 400.f);

            bottompipe->set_position_x(canvas_width);
            bottompipe->set_position_y(pipepos - 400.f);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Se hace que el player dechero se mueva hacia arriba o hacia abajo según el usuario esté
    // tocando la pantalla por encima o por debajo de su centro. Cuando el usuario no toca la
    // pantalla se deja al player quieto.

    void Game_Scene::update_user ()
    {
        if(birdjump)
        {
            bird->set_position_y(bird->get_position_y() + speed);
        }
        else
        {
            bird->set_position_y(bird->get_position_y() - speed - 2);
        }

        if(timer.get_elapsed_seconds() > 1)
        {
            birdjump = false;
        }

        if
        (
            bird->intersects(*top_border)    ||
            bird->intersects(*bottom_border) ||
            bird->intersects(*toppipe)       ||
            bird->intersects(*bottompipe)
        )
        {
            go = false;
            restart_game();
        }
    }

    // ---------------------------------------------------------------------------------------------

    void Game_Scene::render_loading (Canvas & canvas)
    {
        Texture_2D * loading_texture = textures[ID(loading)].get ();

        if (loading_texture)
        {
            canvas.fill_rectangle
                    (
                            { canvas_width * .5f, canvas_height * .5f },
                            { loading_texture->get_width (), loading_texture->get_height () },
                            loading_texture
                    );
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Simplemente se dibujan todos los sprites que conforman la escena.

    void Game_Scene::render_playfield (Canvas & canvas)
    {
        for (auto & sprite : sprites)
        {
            sprite->render (canvas);
        }
    }

}
