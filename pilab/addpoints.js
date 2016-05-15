var cc = 0;
$(function () {                                                                     
    $(document).ready(function() {                                                  
        Highcharts.setOptions({                                                     
            global: {                                                               
                useUTC: false                                                       
            }                                                                       
        });                                                                         
                                                                                    
        var chart;                                                                  
        $('#container').highcharts({                                                
            chart: {                                                                
                type: 'spline',                                                     
                animation: Highcharts.svg, // don't animate in old IE               
                marginRight: 10,                                                    
                events: {                                                           
                    load: function() {                                              
                                                                                  
                        // set up the updating of the chart each second             
                        var series = this.series[0];                                
                        setInterval(function() {                         
                            sender(); 
                            var x = (new Date()).getTime(), // current time         
                                y=cc;
                            series.addPoint([x, y], true, true);                    
                        }, 1000);                                                   
                    }                                                               
                }                                                                   
            },                                                                      
            title: {                                                                
                text: '树莓派温度曲线'                                            
            },                                                                      
            xAxis: {                                                                
                type: 'datetime',                                                   
                tickPixelInterval: 100                                              
            },                                                                      
            yAxis: { 
                // min: -10,
                // max: 80,            
                title: {                                                            
                    text: '摄氏度'                                                   
                },                
                plotLines: [{                                                       
                    value: 0,                                                       
                    width: 1,                                                       
                    color: '#808080'                                                
                }]                                                                  
            },                                                                      
            tooltip: {                                                              
                formatter: function() {                                             
                        return '<b>'+ this.series.name +'</b><br>'+                
                        Highcharts.dateFormat('%Y-%m-%d %H:%M:%S', this.x) +'<br>'+
                        Highcharts.numberFormat(this.y, 2);                         
                }                                                                   
            },                                                                      
            legend: {                                                               
                enabled: false                                                      
            },                                                                      
            exporting: {                                                            
                enabled: false                                                      
            },                                                                      
            series: [{                                                              
                name: 'CPU温度',                                                
                data: (function() {                                                 
                    // generate an array of random data                             
                    var data = [],                                                  
                        time = (new Date()).getTime(),                              
                        i;                                                          
                                                                                    
                    for (i = -50; i <= 0="" 0;="" i++)="" {="" data.push({="" x:="" time="" +="" i="" *="" 1000,="" y:="" math.random()="" });="" }="" return="" data;="" })()="" }]="" function="" createxml()="" var="" xmlhttp;="" if(window.xmlhttprequest)="" xmlhttp="new" xmlhttprequest();="" else="" activexobject("microsoft.xmlhttp");="" 创建异步访问对象="" createxhr()="" xhr;="" try="" xhr="new" activexobject("msxml2.xmlhttp");="" catch="" (e)="" if="" (!xhr="" &&="" typeof="" xmlhttprequest="" !="undefined" )="" 异步访问提交处理="" sender()="" cc="cc++;" (xhr)="" xhr.onreadystatechange="callbackFunction;" test.cgi后面跟个cur_time参数是为了防止ajax页面缓存="" xhr.open("get",="" "cgi-bin="" pycgi_disptemp.py?cur_time=" + new Date().getTime());
        //xhr.open(" get",="" showtemp.cgi?cur_time=");
        xhr.send(null);
    }
    else
    {
        //XMLHttpRequest对象创建失败
        alert(" 浏览器不支持，请更换浏览器！");="" 异步回调函数处理="" callbackfunction()="" (xhr.readystate="=" 4)="" (xhr.status="=" 200)="" returnvalue="xhr.responseText;" (returnvalue="" returnvalue.length=""> 0)
            {
                //cc = returnValue;
                cc = returnValue-0;
                cc = cc*1;
                //return returnValue;
            }
            else
            {
                alert("结果为空！");
            }
        } 
        else 
        {
            alert("页面出现异常！");
        }
    }
}</=>