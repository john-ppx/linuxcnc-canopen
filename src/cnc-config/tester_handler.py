from time import strftime
import linuxcnc
import hal
import gtk
import hal_glib
import pango
import gobject

_X = 0;_Y = 1;_Z = 2;_A = 3;_B = 4;_C = 5;_U = 6;_V = 7;_W = 8

# Mode Notebook Tabs
_MODE_AUTO = 0
_MODE_MAN = 0
_MODE_MDI = 1

# Main Notebook Tabs
_MAIN_PREVIEW = 0
_MAIN_TOOL = 1
_MAIN_OFFSET = 2
_MAIN_ALARM = 3
_MAIN_SETTING = 4
_MAIN_MAINTAIN = 5



class HandlerClass:

    def __init__(self, halcomp,builder,useropts,gscreen):
        
        self.command = linuxcnc.command()
        self.error_channel = linuxcnc.error_channel()
        
        self.stat = linuxcnc.stat()
        self.gscreen = gscreen
        self.emc = gscreen.emc
        self.data = gscreen.data
        self.widgets = gscreen.widgets
        
        #self.data.current_jogincr_index = 7
        
        self.xpos = 0
        self.ypos = 0
        self.width = 0
        self.height = 0
        self.initialized = False

        self.data.sensitive_on_off = ["b", "f_mode", "f_other"]
        self.data.sensitive_AUTO_off = ["f_axis", "f_spindle", "f_coolant", "f_overrides", "f_f", "f_probe"]
        self.data.sensitive_MAN_off = ["f_basic"]

# init
    def initialize_widgets(self):
        self.gscreen.init_show_windows()
        self.gscreen.init_gremlin()
        self.gscreen.init_tooleditor()
        #self.init_offsetpage()
#        self.init_sensitive_on_off()                                                                               #lock button
        self.init_button_colors()
        
        #self.widgets.ntb_main.set_current_page(_MAIN_PREVIEW)
        #self.widgets.ntb_code.set_current_page(_MODE_MAN)
        self.widgets.tb_view_toolpath.set_active(self.gscreen.prefs.getpref("view_tool_path", True, bool))
        self.widgets.tb_view_dimension.set_active(self.gscreen.prefs.getpref("view_dimension", True, bool))
        self.widgets.rb_view_p.emit("toggled")

        
        self.gscreen.sensitize_widgets(self.data.sensitive_on_off, False)
        #self.command.mode(linuxcnc.MODE_MANUAL)
        #self.command.wait_complete()
        self.widgets.tb_status_estop.set_active(True)

    def initialize_pins(self):
        pass


#    def init_sensitive_on_off(self):



# set the button background colors and digits of the DRO
    def init_button_colors(self):
        #self.homed_textcolor = self.gscreen.prefs.getpref("homed_textcolor", "#00FF00", str)     # default green
        #self.unhomed_textcolor = self.gscreen.prefs.getpref("unhomed_textcolor", "#FF0000", str) # default red
        #self.widgets.homed_colorbtn.set_color(gtk.gdk.color_parse(self.homed_textcolor))
        #self.widgets.unhomed_colorbtn.set_color(gtk.gdk.color_parse(self.unhomed_textcolor))
        #self.homed_color = self.gscreen.convert_to_rgb(self.widgets.homed_colorbtn.get_color())
        #self.unhomed_color = self.gscreen.convert_to_rgb(self.widgets.unhomed_colorbtn.get_color())
        self.widgets.tb_status_estop.modify_bg(gtk.STATE_NORMAL, gtk.gdk.color_parse("#00FF00"))
        self.widgets.tb_status_estop.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FF0000"))
        self.widgets.tb_status_on.modify_bg(gtk.STATE_NORMAL, gtk.gdk.color_parse("#FF0000"))
        self.widgets.tb_status_on.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))
        #self.label_home_x.modify_fg(gtk.STATE_NORMAL, gtk.gdk.color_parse("#FF0000"))
        #self.label_home_y.modify_fg(gtk.STATE_NORMAL, gtk.gdk.color_parse("#FF0000"))
        #self.label_home_z.modify_fg(gtk.STATE_NORMAL, gtk.gdk.color_parse("#FF0000"))
        # set the active colours of togglebuttons and radiobuttons
        blue_list = ["tb_coolant_mist", "tb_coolant_flood",
                     "rb_handwheel", "rb_continuous","rb_quantitative",
                     "rb_x","rb_y","rb_z","rb_xy",
                     #"tbtn_units", "tbtn_pause",
                     #"tbtn_optional_stops", "tbtn_optional_blocks",
                     #"rbt_abs", "rbt_rel", "rbt_dtg",
                     "rb_spindle_reverse", "rb_spindle_stop", "rb_spindle_forward"]
        green_list = ["rb_mode_auto", "rb_mode_man", "rb_mode_mdi",
                      "tb_other_set", "tb_other_save", "tb_other_maintain"]
        other_list = ["rb_view_p", "rb_view_x", "rb_view_y", "rb_view_z",
                      "tb_view_dimension", "tb_view_toolpath"]
        for btn in blue_list:
            self.widgets["{0}".format(btn)].modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#44A2CF"))
        for btn in green_list:
            self.widgets["{0}".format(btn)].modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#A2E592"))
        for btn in other_list:
            self.widgets["{0}".format(btn)].modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#BB81B5"))



#signals:  hal*3     x/y/z axis run*6      window size*3
    def connect_signals(self,handlers):
        signal_list = [ ["window1","destroy", "on_window1_destroy"],
                        ["b_show_hal", "clicked", "on_halshow"],
                        ["b_hal_meter", "clicked", "on_halmeter"],
                        ["b_hal_scope", "clicked", "on_halscope"],
                        #["btn_classicladder", "clicked", "on_ladder"],
                        #["btn_status", "clicked", "on_status"],
                        #["btn_calibration", "clicked", "on_calibration"],
                        #["desktop_notify", "toggled", "on_desktop_notify_toggled"],
                        #["theme_choice", "changed", "on_theme_choice_changed"]
                       ]
        for i in signal_list:
            if len(i) == 3:
                self.gscreen.widgets[i[0]].connect(i[1], self.gscreen[i[2]])
            elif len(i) == 4:
                self.gscreen.widgets[i[0]].connect(i[1], self.gscreen[i[2]],i[3])

        self.widgets.full_window.connect("pressed", self.on_fullscreen_pressed)
        self.widgets.max_window.connect("pressed", self.on_max_window_pressed)
        self.widgets.custom_window.connect("pressed", self.on_custom_window_pressed)

        for i in('x','y','z'):
            self.widgets[i+'neg'].connect("pressed", self['jog_'+i],0,True)
            self.widgets[i+'neg'].connect("released", self['jog_'+i],0,False)
            self.widgets[i+'pos'].connect("pressed", self['jog_'+i],1,True)
            self.widgets[i+'pos'].connect("released", self['jog_'+i],1,False)
        #self.widgets.jog_speed.connect("value_changed",self.jog_speed_changed)


#peiodic
    def periodic(self):
        try:
            self.stat.poll()
        except:
            raise SystemExit, "BWC cannot poll linuxcnc status any more"
        error = self.error_channel.poll()
        if error:
            self.gscreen.notify(_("ERROR"), _(error))
        #if self.data.active_gcodes != self.stat.gcodes:
        #    self.gscreen.update_active_gcodes()
        #if self.data.active_mcodes != self.stat.mcodes:
        #    self.gscreen.update_active_mcodes()
        #self._update_vel()
        #self._update_coolant()
        #self._update_spindle()
        #self._update_vc()
        self.clock()
        return True

    def clock(self):
        #self.update_progress()
        #self.update_status()
        #self.update_dro()
        #self.widgets.rbt_rel.set_label(self.system_list[self.stat.g5x_index])
        self.widgets.e_clock.set_text(strftime("%H:%M:%S"))
        return True









    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)





    def on_window1_show(self, widget, data=None):
        self.stat.poll()
        if self.stat.task_state == linuxcnc.STATE_ESTOP:
            self.widgets.tb_status_estop.set_active(True)
            self.widgets.tb_status_on.set_sensitive(False)
        else:
            self.widgets.tb_status_estop.set_active(False)
            self.widgets.tb_status_on.set_sensitive(True)

        #file = self.gscreen.prefs.getpref("open_file", "", str)
        #if file:
        #    self.widgets.file_to_load_chooser.set_filename(file)
        #    self.widgets.hal_action_open.load_file(file)

        start_as = self.gscreen.prefs.getpref("window_geometry", "default", str)
        if start_as == "fullscreen":
            self.widgets.window1.fullscreen()
        elif start_as == "max":
            self.widgets.window1.maximize()
        else:
            self.xpos = int(self.gscreen.prefs.getpref("x_pos", 10, float))
            self.ypos = int(self.gscreen.prefs.getpref("y_pos", 10, float))
            self.width = int(self.gscreen.prefs.getpref("width", 1440, float))
            self.height = int(self.gscreen.prefs.getpref("height", 900, float))
            
            # set the adjustments according to Window position and size
            self.widgets.a_win_xpos.set_value(self.xpos)
            self.widgets.a_win_ypos.set_value(self.ypos)
            self.widgets.a_win_width.set_value(self.width)
            self.widgets.a_win_height.set_value(self.height)
            # move and resize the window
            self.widgets.window1.move(self.xpos, self.ypos)
            self.widgets.window1.resize(self.width, self.height)
        self.initialized = True

# estop machine before closing
    def on_window1_destroy(self, widget, data=None):
        self.command.state(linuxcnc.STATE_OFF)
        self.command.state(linuxcnc.STATE_ESTOP)
        gtk.main_quit()




#**********************************************A*******************************************


    #MAINself.gscreen.sensitize_widgets(self.data.sensitive_MDI_off,True )
    def on_rb_view_p_toggled(self, widget, data=None):
        if self.widgets.rb_view_p.get_active():
            self.widgets.gremlin.set_property("view", "p")
    def on_rb_view_x_toggled(self, widget, data=None):
        if self.widgets.rb_view_x.get_active():
            self.widgets.gremlin.set_property("view", "x")
    def on_rb_view_y_toggled(self, widget, data=None):
        if self.widgets.rb_view_y.get_active():
            self.widgets.gremlin.set_property("view", "y")
    def on_rb_view_z_toggled(self, widget, data=None):
        if self.widgets.rb_view_z.get_active():
            self.widgets.gremlin.set_property("view", "z")

    def on_b_zoom_in_clicked(self, widget, data=None):
        self.widgets.gremlin.zoom_in()
    def on_b_zoom_out_clicked(self, widget, data=None):
        self.widgets.gremlin.zoom_out()

    def on_b_view_clear_clicked(self, widget, data=None):
        self.widgets.gremlin.clear_live_plotter()
    def on_tb_view_dimension_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("show_extents_option", widget.get_active())
        self.gscreen.prefs.putpref("view_dimension", self.widgets.tb_view_dimension.get_active())
    def on_tb_view_toolpath_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("show_live_plot", widget.get_active())
        self.gscreen.prefs.putpref("view_toolpath", self.widgets.tb_view_toolpath.get_active())


    #SETTING
    #window
    def on_fullscreen_pressed(self, widget):
        self.gscreen.prefs.putpref("window_geometry", "fullscreen", str)
        self.widgets.window1.fullscreen()

    def on_max_window_pressed(self, widget):
        self.gscreen.prefs.putpref("window_geometry", "max", str)
        self.widgets.window1.unfullscreen()
        self.widgets.window1.maximize()

    def on_custom_window_pressed(self, widget):
        self.gscreen.prefs.putpref("window_geometry", "default", str)
        self.widgets.window1.unfullscreen()
        self.widgets.window1.unmaximize()
        self.widgets.sb_win_xpos.set_sensitive(True)
        self.widgets.sb_win_ypos.set_sensitive(True)
        self.widgets.window1.move(self.xpos, self.ypos)
        self.widgets.sb_win_width.set_sensitive(True)
        self.widgets.sb_win_height.set_sensitive(True)
        self.widgets.window1.resize(self.width, self.height)

    def on_a_win_xpos_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.gscreen.prefs.putpref("x_pos", value, float)
        self.xpos = value
        self.widgets.window1.move(value, self.ypos)

    def on_a_win_ypos_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.gscreen.prefs.putpref("y_pos", value, float)
        self.ypos = value
        self.widgets.window1.move(self.xpos, value)


    def on_a_win_heigth_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.gscreen.prefs.putpref("height", value, float)
        self.height = value
        self.widgets.window1.resize(self.width, value)

    def on_a_win_width_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.gscreen.prefs.putpref("width", value, float)
        self.width = value
        self.widgets.window1.resize(value, self.height)


#**********************************************B*******************************************

    #STATUS
    def on_tb_status_estop_toggled(self, widget, data=None):
        if widget.get_active():
            self.command.state(linuxcnc.STATE_ESTOP)
            self.command.wait_complete()
            self.stat.poll()
            if self.stat.task_state == linuxcnc.STATE_ESTOP_RESET:
                widget.set_active(False)
            self.widgets.tb_status_on.set_active(False)
        else:
            self.command.state(linuxcnc.STATE_ESTOP_RESET)
            self.command.wait_complete()
            self.stat.poll()
            if self.stat.task_state == linuxcnc.STATE_ESTOP:
                widget.set_active(True)
                self.gscreen.notify(_("ERROR"), _("External ESTOP is set, could not change state!"))

    def on_tb_status_on_toggled(self, widget, data=None):
        if widget.get_active():
            if self.stat.task_state == linuxcnc.STATE_ESTOP:
                widget.set_active(False)
                return
            self.command.state(linuxcnc.STATE_ON)
            self.command.wait_complete()
            self.stat.poll()
            if self.stat.task_state != linuxcnc.STATE_ON:
                widget.set_active(False)
                self.gscreen.notify(_("ERROR"), _("Could not switch the machine on, is limit switch activated?"))
                self.gscreen.sensitize_widgets(self.data.sensitive_on_off, False)
                return
            self.gscreen.sensitize_widgets(self.data.sensitive_on_off, True)
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.widgets.rb_mode_man.set_active(True)
            self.widgets.ntb_code.set_current_page(_MODE_MAN)
        else:
            self.command.state(linuxcnc.STATE_OFF)
            self.gscreen.sensitize_widgets(self.data.sensitive_on_off, False)


    #MODE
    def on_rb_mode_auto_toggled(self, widget, data=None):
        if self.widgets.ntb_main.get_current_page() == _MAIN_SETTING:
            self.widgets.rb_mode_manual.set_active(True)
            self.widgets.rb_mode_auto.set_active(False)
        else:
            self.command.mode(linuxcnc.MODE_AUTO)
            self.command.wait_complete()
            self.widgets.ntb_code.set_current_page(_MODE_AUTO)
            self.gscreen.sensitize_widgets(self.data.sensitive_MAN_off,True )
            self.gscreen.sensitize_widgets(self.data.sensitive_AUTO_off, False)


    def on_rb_mode_man_toggled(self, widget, data=None):
        if self.widgets.ntb_main.get_current_page() == _MAIN_SETTING:
            return
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()
        self.widgets.ntb_code.set_current_page(_MODE_MAN)
        self.gscreen.sensitize_widgets(self.data.sensitive_MAN_off,False )
        self.gscreen.sensitize_widgets(self.data.sensitive_AUTO_off, True)


    def on_rb_mode_mdi_toggled(self, widget, data=None):
        if self.widgets.ntb_main.get_current_page() == _MAIN_SETTING:
            self.widgets.rb_mode_man.set_active(True)
            self.widgets.rb_mode_mdi.set_active(False)
        else:
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            self.widgets.ntb_code.set_current_page(_MODE_MDI)
            self.gscreen.sensitize_widgets(self.data.sensitive_MAN_off,False )
            self.gscreen.sensitize_widgets(self.data.sensitive_AUTO_off, False)





    #OTHER
    def on_tb_other_set_toggled(self, widget, data=None):
        if widget.get_active():
            self.widgets.ntb_main.set_current_page(_MAIN_SETTING)
        else: 
            self.widgets.ntb_main.set_current_page(_MAIN_PREVIEW)

    def on_tb_other_save_toggled(self, widget, data=None):
        if widget.get_active():
            self.widgets.ntb_main.set_current_page(_MAIN_MAINTAIN)
        else: 
            self.widgets.ntb_main.set_current_page(_MAIN_PREVIEW)

    def on_tb_other_maintain_toggled(self, widget, data=None):
        if widget.get_active():
            self.widgets.ntb_main.set_current_page(_MAIN_ALARM)
        else: 
            self.widgets.ntb_main.set_current_page(_MAIN_PREVIEW)



#**********************************************C*******************************************

    #AXIS

    #jog  axis-x -y -z
    def jog_x(self,widget,direction,state):
        self.gscreen.do_key_jog(_X,direction,state)
    def jog_y(self,widget,direction,state):
        self.gscreen.do_key_jog(_Y,direction,state)
    def jog_z(self,widget,direction,state):
        self.gscreen.do_key_jog(_Z,direction,state)
    #def jog_speed_changed(self,widget,value):
    #    self.gscreen.set_jog_rate(absolute = value)
    #def jog_speed_feed_changed(self,widget,value):
    #    self.gscreen.set_jog_rate(absolute = value)
    #def jog_speed_spindle_changed(self,widget,value):
    #    self.gscreen.set_jog_rate(absolute = value)

    def check_mode(self):
        string = []
        string.append(self.data._JOG)
        return string


    #BASIC



    #FENZHONG




    #PROBE



    #SPINDLE
    def on_rb_spindle_forward_clicked(self, widget, data=None):
        self._set_spindle("forward")
    def on_rb_spindle_reverse_clicked(self, widget, data=None):
        self._set_spindle("reverse")
    def on_rb_spindle_stop_clicked(self, widget, data=None):
        self._set_spindle("stop")

    def _set_spindle(self, command):
        #if self.stat.task_state == linuxcnc.STATE_ESTOP:
        #    return
        #if self.stat.task_mode != linuxcnc.MODE_MANUAL:
        #    if self.stat.interp_state == linuxcnc.INTERP_READING or self.stat.interp_state == linuxcnc.INTERP_WAITING:
        #        if self.stat.spindle[0]['direction'] > 0:
        #            self.widgets.rbt_forward.set_sensitive(True)
        #            self.widgets.rbt_reverse.set_sensitive(False)
        #            self.widgets.rbt_stop.set_sensitive(False)
        #        elif self.stat.spindle[0]['direction'] < 0:
        #            self.widgets.rbt_forward.set_sensitive(False)
        #            self.widgets.rbt_reverse.set_sensitive(True)
        #            self.widgets.rbt_stop.set_sensitive(False)
        #        else:
        #            self.widgets.rbt_forward.set_sensitive(False)
        #            self.widgets.rbt_reverse.set_sensitive(False)
        #            self.widgets.rbt_stop.set_sensitive(True)
        #        return
        #rpm = self.check_spindle_range()
        #try:
        #    rpm_out = rpm / self.stat.spindle[0]['override']
        #except:
        #    rpm_out = 0
        if command == "stop":
            self.command.spindle(0)
        elif command == "forward":
        #    self.command.spindle(1, rpm_out)
            self.command.spindle(1)
        elif command == "reverse":
        #    self.command.spindle(-1, rpm_out)
            self.command.spindle(-1)
        else:
            print(_("Something went wrong, we have an unknown spindle widget {0}").format(command))


    #COOLANT
    def on_tb_coolant_flood_toggled(self, widget, data=None):
        #if self.stat.flood and self.widgets.coolant_flood.get_active():
        #    return
        #elif not self.stat.flood and not self.widgets.coolant_flood.get_active():
        #    return
        #el
        if self.widgets.tb_coolant_flood.get_active():
            self.command.flood(linuxcnc.FLOOD_ON)
        else:
            self.command.flood(linuxcnc.FLOOD_OFF)
    def on_tb_coolant_mist_toggled(self, widget, data=None):
        #if self.stat.mist and self.widgets.coolant_mist.get_active():
        #    return
        #elif not self.stat.mist and not self.widgets.coolant_mist.get_active():
        #    return
        #el
        if self.widgets.tb_coolant_mist.get_active():
            self.command.mist(linuxcnc.MIST_ON)
        else:
            self.command.mist(linuxcnc.MIST_OFF)


    #OVERRIDES







#********************************************** *******************************************

#hal status
    def on_hal_status_interp_idle(self, widget):
        print("IDLE")
        if self.load_tool:
            return
        if not self.widgets.tb_status_on.get_active():
            return
        if self.stat.task_state == linuxcnc.STATE_ESTOP or self.stat.task_state == linuxcnc.STATE_OFF:
            self.gscreen.sensitize_widgets(self.data.sensitive_run_idle, False)
        else:
            self.gscreen.sensitize_widgets(self.data.sensitive_run_idle, True)
        if self.tool_change:
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.tool_change = False
        self.current_line = 0

    def on_hal_status_interp_run(self, widget):
        print("RUN")
        self.gscreen.sensitize_widgets(self.data.sensitive_run_idle, False)

    def on_hal_status_state_estop(self, widget=None):
        self.widgets.tb_status_estop.set_active(True)
        self.widgets.tb_status_on.set_sensitive(False)
        self.widgets.tb_status_on.set_active(False)
        #self.widgets.chk_ignore_limits.set_sensitive(False)
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

    def on_hal_status_state_estop_reset(self, widget=None):
        self.widgets.tb_status_estop.set_active(False)
        self.widgets.tb_sttus_on.set_sensitive(True)
        #self.widgets.chk_ignore_limits.set_sensitive(True)
        self._check_limits()

    def on_hal_status_state_off(self, widget):
        if self.widgets.tb_status_on.get_active():
            self.widgets.tb_status_on.set_active(False)
        #self.widgets.chk_ignore_limits.set_sensitive(True)
        self.widgets.tb_status_on.set_label("OFF")

    def on_hal_status_state_on(self, widget):
        if not self.widgets.tb_status_on.get_active():
            self.widgets.tb_status_on.set_active(True)
        #self.widgets.chk_ignore_limits.set_sensitive(False)
        self.widgets.tb_status_on.set_label("ON")
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

    def on_hal_status_mode_manual(self, widget):
        print("MANUAL Mode")
        if self.widgets.ntb_main.get_current_page() == _NB_SETUP:
            return
        self.widgets.rb_mode_man.set_active(True)
        self.widgets.ntb_main.set_current_page(_MAIN_PREVIEW)
        self.widgets.ntb_code.set_current_page(_MODE_MAN)
        self._check_limits()
        self.last_key_event = None, 0

    def on_hal_status_mode_mdi(self, widget):
        if self.tool_change:
            self.widgets.ntb_code.set_current_page(_MODE_MDI)
            return
        if not self.widgets.rb_mode_mdi.get_sensitive():
            self.command.abort()
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.gscreen.notify(_("INFO"), _("It is not possible to change to MDI Mode at the moment"))
            return
        else:
            print("MDI Mode")
            self.widgets.hal_mdihistory.entry.grab_focus()
            self.widgets.ntb_code.set_current_page(_MODE_MDI)
            self.widgets.rb_mode_mdi.set_active(True)
            self.last_key_event = None, 0

    def on_hal_status_mode_auto(self, widget):
        if not self.widgets.rb_mode_auto.get_sensitive():
            self.command.abort()
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.gscreen.notify(_("INFO"), _("It is not possible to change to AUTO Mode at the moment"))
        else:
            print("AUTO Mode")
            self.widgets.ntb_code.set_current_page(_MODE_AUTO)
            self.widgets.rb_mode_auto.set_active(True)
            self.last_key_event = None, 0










def get_handlers(halcomp,builder,useropts,gscreen):
    return [HandlerClass(halcomp,builder,useropts,gscreen)]


'''











'''













