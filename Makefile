xx3dsfml: xx3dsfml.o
	g++ xx3dsfml.o imgui.o imgui_widgets.o imgui_draw.o imgui_tables.o imgui-SFML.o -o xx3dsfml -lftd3xx -lsfml-audio -lsfml-graphics -lsfml-system -lsfml-window -framework OpenGL
	make clean

xx3dsfml.o: delete
	g++ -c xx3dsfml.cpp imgui/imgui.cpp imgui/imgui_widgets.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui-SFML.cpp

clean:
	rm -f xx3dsfml.o imgui.o imgui_widgets.o imgui_draw.o imgui_tables.o imgui-SFML.o

delete: clean
	rm -f xx3dsfml