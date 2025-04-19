#*********************************************************************
#**
#** Name: mvs38_send.awk
#**
#** Desc: Resolve variables in mvs38_send.jcl
#**
#*********************************************************************
{
 gsub("XFRUSER", FRUSER)
 gsub("XTOUSER", TOUSER)
 gsub("XFRNODE", FRNODE)
 gsub("XFID", FID)
 gsub("XDATE", DATE)
 print
}
