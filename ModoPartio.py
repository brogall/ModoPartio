#!/usr/bin/env python

import traceback
from os import path



def double_quote(in_string):
    out_string = '"' + in_string + '"'
    return out_string

def SelectFile():
    partioMode = lx.eval("item.channel partioMode ?")
    if partioMode == 2:         #   particle caching mode
        lx.command( 'dialog.setup',   style = 'fileSave' )
    else:                       #   reading cache
        lx.command( 'dialog.setup',   style = 'fileOpen' )
    lx.command( 'dialog.title', title='Select Cache File' )
    lx.command( 'dialog.fileTypeCustom', format='icecache', username='Softimage ICECACHE', loadPattern="*.icecache;", saveExtension="icecache" )
    lx.command( 'dialog.fileTypeCustom', format='bin', username='Realflow BIN', loadPattern="*.bin", saveExtension="bin" )
    lx.command( 'dialog.fileTypeCustom', format='prt', username='Krakatoa PRT', loadPattern="*.prt", saveExtension="prt" )
    lx.command( 'dialog.fileTypeCustom', format='bgeo', username='Houdini BGEO', loadPattern="*.bgeo", saveExtension="bgeo" )     
    lx.command( 'dialog.fileTypeCustom', format='pdc', username='Maya PDC', loadPattern="*.pdc", saveExtension="pdc" )   
    lx.command( 'dialog.fileTypeCustom', format='pda', username='Maya PDA', loadPattern="*.pda", saveExtension="pda" )  
    lx.command( 'dialog.fileTypeCustom', format='pda', username='Maya PDA', loadPattern="*.pda", saveExtension="pda" )
    try:                                         
        lx.command( 'dialog.open')    
        cache_path =  lx.eval(  'dialog.result ?' )
        cache_path = path.abspath(cache_path)
        return cache_path
    except:
        return None

def main():
    try:
        args = lx.args()

        if args[0] == "SelectFile":
            cache_path = SelectFile()
            if(cache_path != None):
                lx.eval('item.channel cacheFileName ' + double_quote(cache_path))


    except:
        lx.out( "Command failed with ", sys.exc_info()[0] )
        lx.out(traceback.format_exc())    

main()