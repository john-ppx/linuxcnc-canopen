

import linuxcnc
import hal
import gtk

_X = 0;_Y = 1;_Z = 2;_A = 3;_B = 4;_C = 5;_U = 6;_V = 7;_W = 8


class HandlerClass:

    def __init__(self, halcomp,builder,useropts,gscreen):
        self.command = linuxcnc.command()
        self.emc = gscreen.emc
        self.data = gscreen.data
        self.widgets = gscreen.widgets
        self.gscreen = gscreen
        self.data.current_jogincr_index = 7



    #gremlin view
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


    #spindle
    def on_spindle_R_clicked(self, widget, data=None):
        self._set_spindle("forward")

    def on_spindle_L_clicked(self, widget, data=None):
        self._set_spindle("reverse")

    def on_spindle_S_clicked(self, widget, data=None):
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


    #show DRO
    def on_cb_show_dro_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("enable_dro", widget.get_active())
        self.gscreen.prefs.putpref("enable_dro", widget.get_active())
        self.widgets.cb_show_dtg.set_sensitive(widget.get_active())
        self.widgets.cb_show_offsets.set_sensitive(widget.get_active())

    def on_cb_show_dtg_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("show_dtg", widget.get_active())
        self.gscreen.prefs.putpref("show_dtg", widget.get_active())

    def on_cb_show_offsets_toggled(self, widget, data=None):
        self.widgets.gremlin.show_offsets = widget.get_active()
        self.gscreen.prefs.putpref("show_offsets", widget.get_active())


    #axis-x -y -z & spindle_speed
    def jog_x(self,widget,direction,state):
        self.gscreen.do_key_jog(_X,direction,state)
    def jog_y(self,widget,direction,state):
        self.gscreen.do_key_jog(_Y,direction,state)
    def jog_z(self,widget,direction,state):
        self.gscreen.do_key_jog(_Z,direction,state)
    def jog_speed_changed(self,widget,value):
        self.gscreen.set_jog_rate(absolute = value)

    #def jog_speed_feed_changed(self,widget,value):
    #    self.gscreen.set_jog_rate(absolute = value)
    def jog_speed_spindle_changed(self,widget,value):
        self.gscreen.set_jog_rate(absolute = value)

    def check_mode(self):
        string = []
        string.append(self.data._JOG)
        return string

    def connect_signals(self,handlers):
        signal_list = [ ["window1","destroy", "on_window1_destroy"],
                        ["btn_show_hal", "clicked", "on_halshow"],
                        ["btn_hal_meter", "clicked", "on_halmeter"],
                        ["btn_hal_scope", "clicked", "on_halscope"],
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
        #self.widgets.full_window.connect("pressed", self.on_fullscreen_pressed)
        #self.widgets.max_window.connect("pressed", self.on_max_window_pressed)
        #self.widgets.default_window.connect("pressed", self.on_default_window_pressed)

        for i in('x','y','z'):
            self.widgets[i+'neg'].connect("pressed", self['jog_'+i],0,True)
            self.widgets[i+'neg'].connect("released", self['jog_'+i],0,False)
            self.widgets[i+'pos'].connect("pressed", self['jog_'+i],1,True)
            self.widgets[i+'pos'].connect("released", self['jog_'+i],1,False)
        self.widgets.jog_speed.connect("value_changed",self.jog_speed_changed)

        #self.widgets.jog_speed_feed.connect("value_changed",self.jog_speed_feed_changed)
        self.widgets.jog_speed_spindle.connect("value_changed",self.jog_speed_spindle_changed)


    #coolant
    def on_coolant_flood_toggled(self, widget, data=None):
        #if self.stat.flood and self.widgets.coolant_flood.get_active():
        #    return
        #elif not self.stat.flood and not self.widgets.coolant_flood.get_active():
        #    return
        #el
        if self.widgets.coolant_flood.get_active():
            self.command.flood(linuxcnc.FLOOD_ON)
        else:
            self.command.flood(linuxcnc.FLOOD_OFF)

    def on_coolant_mist_toggled(self, widget, data=None):
        #if self.stat.mist and self.widgets.coolant_mist.get_active():
        #    return
        #elif not self.stat.mist and not self.widgets.coolant_mist.get_active():
        #    return
        #el
        if self.widgets.coolant_mist.get_active():
            self.command.mist(linuxcnc.MIST_ON)
        else:
            self.command.mist(linuxcnc.MIST_OFF)


    def initialize_widgets(self):
        self.gscreen.init_show_windows()
    def initialize_pins(self):
        pass
    def periodic(self):
        pass


    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

def get_handlers(halcomp,builder,useropts,gscreen):
    return [HandlerClass(halcomp,builder,useropts,gscreen)]

'''
    def on_keycall_XPOS(self,state,SHIFT,CNTRL,ALT):
        widget = self.widgets.xpos
        if not self.check_kb_shortcuts(): return
        if state:
            self.on_xpos_pressed(widget)
        else:
            self.on_xpos_released(widget)

    def on_keycall_XNEG(self,state,SHIFT,CNTRL,ALT):
        widget = self.widgets.xneg
        if not self.check_kb_shortcuts(): return
        if state:
            self.on_btn_jog_pressed(widget)
        else:
            self.on_btn_jog_released(widget)

    def on_keycall_YPOS(self,state,SHIFT,CNTRL,ALT):
        widget = self.widgets.btn_y_plus
        if not self.check_kb_shortcuts(): return
        if state:
            self.on_btn_jog_pressed(widget)
        else:
            self.on_btn_jog_released(widget)

    def on_keycall_YNEG(self,state,SHIFT,CNTRL,ALT):
        widget = self.widgets.btn_y_minus
        if not self.check_kb_shortcuts(): return
        if state:
            self.on_btn_jog_pressed(widget)
        else:
            self.on_btn_jog_released(widget)

    def on_keycall_ZPOS(self,state,SHIFT,CNTRL,ALT):
        widget = self.widgets.btn_z_plus
        if not self.check_kb_shortcuts(): return
        if state:
            self.on_btn_jog_pressed(widget)
        else:
            self.on_btn_jog_released(widget)

    def on_keycall_ZNEG(self,state,SHIFT,CNTRL,ALT):
        widget = self.widgets.btn_z_minus
        if not self.check_kb_shortcuts(): return
        if state:
            self.on_btn_jog_pressed(widget)
        else:
            self.on_btn_jog_released(widget)

    def init_jog_increments(self):
        increments = self.gscreen.inifile.find("DISPLAY", "INCREMENTS")
        if increments:
            if "," in increments:
                for i in increments.split(","):
                    self.jog_increments.append(i.strip())
            else:
                self.jog_increments = increments.split()
            self.jog_increments.insert(0, 0)
        else:
            self.jog_increments = [0, "1.000", "0.100", "0.010", "0.001"]
            print("No jog increments found in [DISPLAY] of INI file, Using default values")
        if len(self.jog_increments) > 10:
            print(_("Increment list shortened to 10"))
            self.jog_increments = self.jog_increments[0:11]
        self.jog_increments.pop(0)
        model = self.widgets.cmb_increments.get_model()
        model.clear()
        model.append(["Continuous"])
        for index, inc in enumerate(self.jog_increments):
            model.append((inc,))
        self.widgets.cmb_increments.set_active(0)



    def on_tb_estop_toggled(self, widget, data=None):
        if widget.get_active():  # estop is active, open circuit
            self.command.state(linuxcnc.STATE_ESTOP)
            self.command.wait_complete()
            self.stat.poll()
            if self.stat.task_state == linuxcnc.STATE_ESTOP_RESET:
                widget.set_active(False)
        else:  # estop circuit is fine
            self.command.state(linuxcnc.STATE_ESTOP_RESET)
            self.command.wait_complete()
            self.stat.poll()
            if self.stat.task_state == linuxcnc.STATE_ESTOP:
                widget.set_active(True)
                self.gscreen.notify(_("ERROR"), _("External ESTOP is set, could not change state!"), ALERT_ICON)

    def on_tb_on_toggled(self, widget, data=None):
        if widget.get_active():    # from Off to On
            if self.stat.task_state == linuxcnc.STATE_ESTOP:
                widget.set_active(False)
                return
            self.command.state(linuxcnc.STATE_ON)
            self.command.wait_complete()
            self.stat.poll()
            if self.stat.task_state != linuxcnc.STATE_ON:
                widget.set_active(False)
                self.gscreen.notify(_("ERROR"), _("Could not switch the machine on, is limit switch activated?"), ALERT_ICON)
                self.gscreen.sensitize_widgets(self.data.sensitive_on_off, False)
                return
            self.gscreen.sensitize_widgets(self.data.sensitive_on_off, True)
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.widgets.rbt_manual.set_active(True)
            self.widgets.ntb_mode.set_current_page(_MODE_MANUAL)
        else:    # from On to Off
            self.command.state(linuxcnc.STATE_OFF)
            self.gscreen.sensitize_widgets(self.data.sensitive_on_off, False)
'''



















